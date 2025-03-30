local scene = require('scene')

do
    local e = scene.create_static_actor("Sung/axes.dun/axes.dmd")
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_static_actor("Sung/city.dun/city.dmd")
    e:get_transform():set_pos(-21, 1.8, 40.7)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_static_actor("Sung/bistro.dun/Bistro_v5_2.dmd")
    e:get_transform():set_pos(-100, 2, -39)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_skinned_actor("Sung/Character Running.dun/Character Running.dmd")

    local a = e:get_anim_state()
    a:set_anim_idx(0)

    local t = e:get_transform()
    t:set_pos(-112, 2, -39)
    t:set_scale(0.3)

    print("Skinned actor created:", e:get_id(), e:get_respath())
end
