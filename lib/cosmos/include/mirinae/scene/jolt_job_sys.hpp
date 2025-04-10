#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystem.h>


namespace mirinae {

    std::unique_ptr<JPH::JobSystem> create_jolt_job_sys();

}
