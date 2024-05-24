local scene = require('scene')

do
    local e = scene.create_static_model("asset/models/sponza/sponza.dmd")
    print("Static actor created:", e:get_respath())
end

do
    local e = scene.create_skinned_model("ThinMatrix/Character Running.dmd")
    print("Skinned actor created:", e:get_respath())
end
