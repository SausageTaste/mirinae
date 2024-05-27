local scene = require('scene')

do
    local e = scene.create_static_actor("asset/models/sponza/sponza.dmd")
    e:get_transform():set_scale(0.01, 0.01, 0.01)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_skinned_actor("ThinMatrix/Character Running.dmd")
    e:get_transform():set_scale(0.15, 0.15, 0.15)
    print("Skinned actor created:", e:get_id(), e:get_respath())
end

--[[
do
    bard = scene.create_skinned_actor("Sung/bard/bard_subset.dmd")
    local t = bard:get_transform()
    t:set_pos(1, 0, 0)
    t:rotate(-90, 0, 1, 0)
    print("Skinned actor created:", bard:get_id(), bard:get_respath())
end

do
    artist = scene.create_skinned_actor("Sung/artist/artist_subset.dmd")
    artist:set_anim_idx(10)
    artist:get_transform():set_pos(1.35, 0, 0)
    local t = artist:get_transform()
    t:set_pos(2, 0, 0)
    t:rotate(-90, 0, 1, 0)
    print("Skinned actor created:", artist:get_id(), artist:get_respath())
end
]]--
