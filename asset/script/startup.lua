scene = require('scene')

do
    local e = scene.create_static_actor("Sung/axes.dun/axes.dmd")
    e:get_transform():set_scale(1)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    sponza = scene.create_static_actor("Sung/sponza.dun/sponza.dmd")
    sponza:get_transform():set_pos(27, 1, -18)
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
    t:set_pos(-59.000, 7.750, -50.000)
    t:set_scale(0.14)

    print("Skinned actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_skinned_actor("Sung/artist.dun/artist_subset.dmd")

    local a = e:get_anim_state()
    a:set_anim_name("idle_normal_1")

    local t = e:get_transform()
    t:set_pos(-58.300, 7.750, -50.000)
    t:rotate(-90, 0, 1, 0)

    print("Skinned actor created:", e:get_id(), e:get_respath())
end
