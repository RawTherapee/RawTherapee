#include "tgmr/training_identity.h"

#include "tgmr/sha256.h"
#include "tgmr/training_identity_config.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace tgmr
{

const char *trainingBackendName(TrainingBackend backend)
{
    switch (backend) {
        case TrainingBackend::CANONICAL: return "canonical";
        case TrainingBackend::CPU: return "cpu";
        case TrainingBackend::OMP_TARGET: return "omp-target";
    }
    throw std::runtime_error("unknown TGMR training backend");
}

std::string canonicalFitConfigurationJson(const FitConfiguration &configuration)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"backend\": \"" << trainingBackendName(configuration.backend) << "\",\n"
        << "  \"batch_size\": " << configuration.batchSize << ",\n"
        << "  \"components\": " << configuration.components << ",\n"
        << "  \"covariance_floor\": " << configuration.covarianceFloor << ",\n"
        << "  \"degrees_of_freedom\": " << configuration.degreesOfFreedom << ",\n"
        << "  \"device\": " << configuration.device << ",\n"
        << "  \"format\": \"rawtherapee-tgmr-fit-configuration-v1\",\n"
        << "  \"gaussian_iterations\": " << configuration.gaussianIterations << ",\n"
        << "  \"gpu_yield_milliseconds\": "
        << configuration.gpuYieldMilliseconds << ",\n"
        << "  \"maximum_bytes\": " << configuration.maximumBytes << ",\n"
        << "  \"seed\": \"0x" << std::hex << std::setfill('0') << std::setw(16)
        << configuration.seed << std::dec << "\",\n"
        << "  \"source_limit\": " << configuration.sourceLimit << ",\n"
        << "  \"student_iterations\": " << configuration.studentIterations << "\n"
        << "}\n";
    return output.str();
}

std::array<std::uint8_t, 32> fitConfigurationSha256(
    const FitConfiguration &configuration)
{
    const std::string json = canonicalFitConfigurationJson(configuration);
    return sha256(json.data(), json.size());
}

std::array<std::uint8_t, 32> trainerRevisionSha256()
{
    return parseSha256(TGMR_TRAINER_REVISION_SHA256);
}

std::string canonicalTrainingIdentityJson(const FitConfiguration &configuration)
{
    std::string config = canonicalFitConfigurationJson(configuration);
    if (!config.empty() && config.back() == '\n') config.pop_back();
    std::ostringstream output;
    output << "{\n  \"configuration\": ";
    for (std::size_t index = 0; index < config.size(); ++index) {
        output << config[index];
        if (config[index] == '\n') output << "  ";
    }
    output << ",\n  \"configuration_sha256\": \""
        << hex(fitConfigurationSha256(configuration)) << "\",\n"
        << "  \"format\": \"rawtherapee-tgmr-training-identity-v1\",\n"
        << "  \"trainer_revision_sha256\": \""
        << hex(trainerRevisionSha256()) << "\"\n}\n";
    return output.str();
}

} // namespace tgmr
