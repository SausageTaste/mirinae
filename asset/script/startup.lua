local scene = require('scene')

do
    local e = scene.create_static_model("asset/models/sponza/sponza.dmd")
    e:get_transform():set_scale(0.01, 0.01, 0.01)
    print("Static actor created:", e:get_respath())
end

do
    local e = scene.create_skinned_model("ThinMatrix/Character Running.dmd")
    e:get_transform():set_scale(0.15, 0.15, 0.15)
    print("Skinned actor created:", e:get_respath())
end

do
    local e = scene.create_skinned_model("Sung/bard/bard_subset.dmd")
    e:get_transform():set_pos(1, 0, 0)
    print("Skinned actor created:", e:get_respath())
end

do
    local e = scene.create_skinned_model("Sung/artist/artist_subset.dmd")
    e:get_transform():set_pos(1.5, 0, 0)
    print("Skinned actor created:", e:get_respath())
end
