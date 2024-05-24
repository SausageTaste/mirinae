local scene = require('scene')

do
    local e = scene.create_static_model("asset/models/sponza/sponza.dmd")
    e:get_transform():set_scale(0.01, 0.01, 0.01)
    print("Static actor created:", e:get_respath())
end

do
    local e = scene.create_skinned_model("ThinMatrix/Character Running.dmd")
    e:get_transform():set_scale(0.6, 0.6, 0.6)
    print("Skinned actor created:", e:get_respath())
end
