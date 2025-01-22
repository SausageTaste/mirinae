scene = require('scene')

do
    local e = scene.create_static_actor("Sung/axes.dun/axes.dmd")
    e:get_transform():set_scale(1)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    sponza = scene.create_static_actor("Sung/sponza.dun/sponza.dmd")
    sponza:get_transform():set_pos(30, 1, -70)
    sponza:get_transform():set_scale(0.01)
    print("Static actor created:", sponza:get_id(), sponza:get_respath())
end

do
    city = scene.create_static_actor("Sung/city.dun/city.dmd")
    city:get_transform():set_scale(0.5)
    print("Static actor created:", city:get_id(), city:get_respath())
end

do
    local e = scene.create_static_actor("Sung/cerberus.dun/Cerberus_LP.dmd")
    e:get_transform():set_pos(0.5, 2, 0.5)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_skinned_actor("Sung/Character Running.dun/Character Running.dmd")

    local a = e:get_anim_state()
    a:set_anim_idx(0)

    local t = e:get_transform()
    t:set_pos(3, 0, -0.5)
    t:rotate(-90, 0, 1, 0)
    t:set_scale(0.15)

    print("Skinned actor created:", e:get_id(), e:get_respath())
end

do
    bard = scene.create_skinned_actor("Sung/bard.dun/bard_subset.dmd")

    local a = bard:get_anim_state()
    a:set_anim_name("idle_normal_1")

    local t = bard:get_transform()
    t:set_pos(-2.5, 0, 0.5)
    t:rotate(0, 0, 1, 0)

    print("Skinned actor created:", bard:get_id(), bard:get_respath())
end

do
    artist = scene.create_skinned_actor("Sung/artist.dun/artist_subset.dmd")

    local a = artist:get_anim_state()
    a:set_anim_name("idle_normal_1")

    local t = artist:get_transform()
    t:set_pos(-2.5, 0, 0)
    t:rotate(0, 0, 1, 0)

    print("Skinned actor created:", artist:get_id(), artist:get_respath())
end

do
    slayer = scene.create_skinned_actor("Sung/slayer.dun/slayer_subset.dmd")

    local a = slayer:get_anim_state()
    a:set_anim_name("idle_normal_1")

    local t = slayer:get_transform()
    t:set_pos(-2.5, 0, -0.5)
    t:rotate(0, 0, 1, 0)

    print("Skinned actor created:", slayer:get_id(), slayer:get_respath())
end

do
    cherno = scene.create_skinned_actor("Sung/cherno.dun/cherno_swimsuit.dmd")

    local a = cherno:get_anim_state()
    a:set_anim_name("pose_01")

    local t = cherno:get_transform()
    t:set_pos(-2.5, 0, -1)
    t:rotate(90, 0, 1, 0)
    t:set_scale(0.65)

    print("Static actor created:", cherno:get_id(), cherno:get_respath())
end


function cycle_anim(offset)
    actors = {bard, artist, slayer}

    for _, actor in ipairs(actors) do
        local a = actor:get_anim_state()
        local idx = a:get_cur_anim_idx()
        if idx ~= nil then
            a:set_anim_idx((idx + offset) % a:get_anim_count())
        end
    end
end


function set_anim_speed(speed)
    actors = {bard, artist, slayer}

    for _, actor in ipairs(actors) do
        local a = actor:get_anim_state()
        a:set_anim_speed(speed)
    end
end


function cam_pos()
    print(scene.get_cam_pos())
    print(scene.get_cam_dir())
end
