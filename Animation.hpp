#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <functional>
#include <map>
#include <vector>

struct Keyframe {
    float time;
    uint32_t start;
    uint32_t count;
};
static_assert(sizeof(Keyframe) == 12);

template<typename T>
struct Animation {
    std::string name;
    uint32_t fps;
    bool loop = false;

    float get_anim_length() const {
        if (keyframes.size() == 0) return 0;
        return keyframes.back().time;
    } 

    uint32_t get_keyframe_count() const {
        return (uint32_t)keyframes.size();
    }

    uint32_t get_actors_count_at(uint32_t i) const {
        return keyframes[i].count;
    }

    bool add_keyframes(const std::vector< T > &buffer, 
        const std::vector< float > &times,
        const std::vector< std::string > &property_names={}); 

    void set_loop (bool do_loop);


    Animation(std::string name, uint32_t fps, bool loop) : name(name), fps(fps), loop(loop) { };

    // -- internals --
    std::vector< T > data;
    std::vector< Keyframe > keyframes;
    std::vector < std::string > index_to_name;
    std::map < std::string, uint32_t > name_to_index;
};

template<typename T>
struct AnimationBuffer {
    AnimationBuffer(std::string const &filename);
    const Animation< T > &lookup(std::string const &name) const;

    std::map< std::string, Animation< T > > animations;
};

template<typename T>
struct AnimationGraph {
    typedef std::pair< std::function< bool(AnimationGraph< T > &) >, std::string > Transition;

    struct State {
        const Animation< T > &animation;
        std::vector< Transition > transitions;

        State(const Animation < T > &animation) : animation(animation) { };
    };

    State *current_state = nullptr;
    uint32_t keyframe_index = 0;

    // ranges fro 0-1
    float playback = 0;

    // requires a user-defined interpolation function as input
    AnimationGraph(std::function< T(const T&, const T&, float) > interp) : interp(interp) { };

    bool add_state(const Animation< T > &animation);
    bool add_transition(std::string state, Transition transition);

    void update(float elapsed);
    // returns map of actor name to new data
    std::map< std::string, T > sample();

    // -- internals
    std::map< std::string, State > states;
    std::function< T(const T&, const T&, float) > interp = nullptr;
};

#include "read_write_chunk.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

template <typename T>
void Animation< T >::set_loop(bool do_loop) {
    loop = do_loop;
}

template <typename T>
bool Animation< T >::add_keyframes(const std::vector< T > &buffer, const std::vector< float > &times, const std::vector< std::string > &property_names) {
    // check that data sizes are as expected
    uint32_t expected_count = 0;
    if (index_to_name.empty()) {
        if (property_names.empty()) {
            std::cerr << "WARNING: first call to add_keyframes for any Animation must specify property_names" << std::endl;
            return false;
        }
        expected_count = (uint32_t) property_names.size();
    } 
    else {
        if (property_names.size() > 0) {
            if (property_names.size() != index_to_name.size()) {
                std::cerr << "WARNING: numbers of property names does not follow convention of previously inserted data." << std::endl;
                return false;
            }

            for (uint32_t i = 0; i < index_to_name.size(); i++) {
                if (property_names[i] != index_to_name[i]) {
                    std::cerr << "WARNING: property names at position " << i << " does not match previous assigned data with name " << index_to_name[i] << std::endl;
                    return false;
                }
            }
        }
        expected_count = (uint32_t) index_to_name.size();
    }

    if (expected_count == 0) {
        std::cerr << "WARNING: cannot safely continue because 0 property names were determined" << std::endl;
        return false;   
    }

    if (times.empty() || buffer.size() != times.size() * expected_count) {
        std::cerr << "WARNING: buffer.size() must be equal to property_names.size() * times.size()" << std::endl;
        return false;
    }

    uint32_t expected_start = keyframes.empty() ? 0 : keyframes.back().start + keyframes.back().count;
    float prev_time = keyframes.empty() ? -1 : keyframes.back().time;
    for (float time : times) {
        if (prev_time >= time || time < 0.f) {
            std::cerr << "WARNING: appending new_keyframes to existing keyframes does not produce keyframes with nonnegative, strictly increasing times!" << std::endl;
            return false;
        }

        keyframes.emplace_back();
        Keyframe &keyframe = keyframes.back();
        keyframe.time = time;
        keyframe.start = expected_start;
        keyframe.count = expected_count;

        prev_time = time;
        expected_start += expected_count;    
    }

    data.insert(data.end(), buffer.begin(), buffer.end());

    if (index_to_name.empty()) {
        index_to_name = property_names;
    }

    return true;
} 

template <typename T>
AnimationBuffer< T >::AnimationBuffer(std::string const &filename) {
    std::ifstream file(filename, std::ios::binary);

    std::vector< T > data;

    if (filename.size() >= 5 && filename.substr(filename.size()-5) == ".anim") {
        read_chunk(file, "anim", &data);
    } else {
		throw std::runtime_error("Unknown file type '" + filename + "'");
	}

    std::vector< char > strings;
	read_chunk(file, "str0", &strings);

    std::vector< uint32_t > actor_cts;
    read_chunk(file, "act0", &actor_cts);

    std::vector< uint32_t > fps_vec;
    read_chunk(file, "fps0", &fps_vec);
    uint32_t fps = fps_vec[0];

    {
        struct AnimationIndexEntry {
            uint32_t name_begin, name_end;
            uint32_t keyframe_begin, keyframe_end;
        };
        static_assert(sizeof(AnimationIndexEntry) == 16);

        struct ActorIndexEntry {
            uint32_t actor_begin, actor_end;
        };
        static_assert(sizeof(ActorIndexEntry) == 8);


        std::vector< AnimationIndexEntry > anim_index;
        read_chunk(file, "idx0", &anim_index);
        std::vector< ActorIndexEntry > actor_index;
        read_chunk(file, "idx1", &actor_index);

        std::vector< Keyframe > keyframes;
        read_chunk(file, "keys", &keyframes);

        assert(actor_cts.size() == anim_index.size());
        
        uint32_t keyframe_index = 0;
        for (size_t i = 0; i < anim_index.size(); ++i) {
            auto const &anim_entry = anim_index[i];
            if(!(anim_entry.name_begin <= anim_entry.name_end && anim_entry.name_end <= strings.size())) {
				throw std::runtime_error("index entry has out-of-range name begin/end");
            }
            if(!(anim_entry.keyframe_begin <= anim_entry.keyframe_end && anim_entry.keyframe_end <= data.size())) {
				throw std::runtime_error("index entry has out-of-range keyframe begin/end");
            } 

            std::string name(&strings[0] + anim_entry.name_begin, &strings[0] + anim_entry.name_end);
            Animation< T > anim(name, fps, true);
            anim.data.assign(
                data.begin() + anim_entry.keyframe_begin,
                data.begin() + anim_entry.keyframe_end
            );
            uint32_t actor_count = actor_cts[i];
            uint32_t key_count = (anim_entry.keyframe_end - anim_entry.keyframe_begin) / actor_count;
            assert(key_count * actor_count == anim_entry.keyframe_end - anim_entry.keyframe_begin);
            anim.keyframes.assign(
                keyframes.begin() + keyframe_index,
                keyframes.begin() + keyframe_index + key_count
            );        
            assert(anim.keyframes.size() == key_count);
            assert(anim.keyframes[0].start == 0);

            anim.index_to_name.resize(actor_count);
            for (uint32_t k = 0; k < actor_count; ++k) {
                auto const &actor_entry = actor_index[k];
                std::string actor_name(&strings[0] + actor_entry.actor_begin, &strings[0] + actor_entry.actor_end);
                if (!anim.name_to_index.insert(std::make_pair(actor_name, k)).second) {
                    std::cerr << "WARNING, SHOULD NOT HAVE ARRIVED HERE: actor name '" + actor_name + "' in animation '" + name + "' in filename '" + filename + "' collides with existing actor." << std::endl;
                }
                else {
                    anim.index_to_name[k] = actor_name;
                }
            }

            if (!animations.insert(std::make_pair(name, anim)).second) {
				std::cerr << "WARNING: animation name '" + name + "' in filename '" + filename + "' collides with existing animation." << std::endl;
                continue;
            }

            keyframe_index += key_count;
        }
    }
}

template <typename T>
const Animation< T > &AnimationBuffer< T >::lookup(std::string const &name) const {
    auto a = animations.find(name);
    if (a == animations.end()) {
		throw std::runtime_error("Looking up animation '" + name + "' that doesn't exist.");
    }
    return a->second;
}

template <typename T>
bool AnimationGraph< T >::add_state(const Animation< T > &animation) {
    bool inserted = states.insert(std::make_pair(animation.name, State(animation))).second;
    if (!inserted) {
        std::cerr << "WARNING: state with name " << animation.name << " already exists in graph...skipping" << std::endl;
        return false;
    }

    if (!current_state) current_state = &(states.find(animation.name)->second);
    return true;
}

template <typename T>
bool AnimationGraph< T >::add_transition(std::string name, Transition transition) {
    auto it = states.find(name);
    if (it == states.end()) {
        std::cerr << "WARNING: state with name " << name << " does not exist in graph...skipping" << std::endl;
        return false;   
    }
    (it->second).transitions.emplace_back(transition);
    return true;
}

template <typename T>
void AnimationGraph< T >::update(float elapsed) {
    if (!current_state) {
        std::cerr << "WARNING: No data exists in this animation graph" << std::endl;
        return;
    }

    const Animation< T > &animation = current_state->animation;
    float anim_length = animation.get_anim_length();
    size_t key_count = animation.get_keyframe_count();

    if (playback < anim_length) {
        playback += elapsed;
    }

    if (playback >= anim_length) {
        if (animation.loop) {
            playback = 0;
            keyframe_index = 0;
        }
        else {
            playback = std::min(playback, anim_length);
        }
    }

    while (keyframe_index < key_count - 1 && playback >= animation.keyframes[keyframe_index + 1].time) 
        keyframe_index++;

    for (auto transition : current_state->transitions) {
        if ((transition.first)(*this)) {
            auto it = states.find(transition.second);
            if (it == states.end()) {
                std::cerr << "WARNING: could not find state with name " << transition.second << " to transition into staying in current state." << std::endl; 
                continue;
            }
            current_state = &(it->second);
            playback = 0.f;
            keyframe_index = 0;
            return;
        }
    }
}

template <typename T>
std::map< std::string, T > AnimationGraph< T >::sample() {
    if (!current_state) {
        std::cerr << "WARNING: No data exists in this animation graph" << std::endl;
        return {};
    }

    if (!interp) {
        std::cerr << "WARNING: No interpolation function exists for this animation graph" << std::endl;
        return {};
    }

    const Animation< T > &animation = current_state->animation;
    float anim_length = animation.get_anim_length();
    uint32_t key_count = animation.get_keyframe_count();
    if (key_count <= 0) {
        std::cerr << "WARNING: attempted to sample points from empty animation with name " + animation.name << std::endl;
        return {};
    }
    
    uint32_t next_key_index = animation.loop ? 
        (keyframe_index + 1) % key_count :
        std::min(key_count - 1, keyframe_index + 1);

    const Keyframe &curr = animation.keyframes[keyframe_index];
    const Keyframe &next = animation.keyframes[next_key_index];

    float t = 0.f;
    if (keyframe_index != next_key_index) {
        float dt = next.time - curr.time;
        if (dt < 0.f) dt += anim_length;
        t = playback - curr.time;
        if (t < 0.f) t += anim_length;

        t /= dt;
    } else t = 1.f;

    assert(curr.count == next.count);
    std::map < std::string, T > res;
    for (uint32_t i = 0; i < curr.count; i++) {
        res.insert(std::make_pair(animation.index_to_name[i], 
            interp(animation.data[curr.start + i], animation.data[next.start + i], t)));
    }

    return res;
}