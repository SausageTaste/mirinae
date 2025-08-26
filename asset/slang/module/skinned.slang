module skinned;


public static const int MAX_JOINTS = 128;


public float4x4 make_joint_transform(int4 j_ids, float4 j_weights, float4x4 j_tforms[MAX_JOINTS]) {
    float4x4 joint_transform = float4x4(0);
    for (int i = 0; i < 4; i++) {
        let idx = clamp(j_ids[i], 0, MAX_JOINTS);
        joint_transform += j_weights[i] * j_tforms[idx];
    }

    return joint_transform;
}
