# Reviewed data-only TGMR model. Update these identities only after reviewing
# a replacement model and its provenance. Custom weights use the runtime
# RT_XTRANS_TGMR_MODEL override, not an alternate trusted build-time hash.
set(TGMR_OFFICIAL_MODEL
    "${PROJECT_SOURCE_DIR}/rtdata/models/xtrans-tgmr-v2.tgmr" CACHE FILEPATH
    "Reviewed bundled TGMR model (empty to build without installing weights)")
set(TGMR_OFFICIAL_MODEL_SHA256
    "5707fbd67d1998ed3bac646ecce967297a2022776821d62944a24dbbb8615285")
set(TGMR_OFFICIAL_MODEL_MANIFEST
    "${PROJECT_SOURCE_DIR}/rtdata/models/xtrans-tgmr-v2.tgmr.json")
set(TGMR_CORPUS_ATTRIBUTION
    "${PROJECT_SOURCE_DIR}/rtdata/models/xtrans-tgmr-CORPUS-NOTICE.txt")
set(TGMR_MODEL_LICENSE
    "${PROJECT_SOURCE_DIR}/rtdata/models/xtrans-tgmr-LICENSE.txt")

add_compile_definitions(RT_TGMR_OFFICIAL_V2_SHA256="${TGMR_OFFICIAL_MODEL_SHA256}")
if(TGMR_OFFICIAL_MODEL)
    foreach(artifact IN ITEMS TGMR_OFFICIAL_MODEL TGMR_OFFICIAL_MODEL_MANIFEST
            TGMR_CORPUS_ATTRIBUTION TGMR_MODEL_LICENSE)
        if(NOT EXISTS "${${artifact}}")
            message(FATAL_ERROR "Missing TGMR release artifact: ${${artifact}}")
        endif()
    endforeach()
    file(SIZE "${TGMR_OFFICIAL_MODEL}" model_size)
    file(SHA256 "${TGMR_OFFICIAL_MODEL}" model_hash)
    file(SHA256 "${TGMR_OFFICIAL_MODEL_MANIFEST}" manifest_hash)
    file(SHA256 "${TGMR_CORPUS_ATTRIBUTION}" notice_hash)
    if(NOT model_size EQUAL 6073768 OR NOT model_hash STREQUAL TGMR_OFFICIAL_MODEL_SHA256)
        message(FATAL_ERROR "TGMR official model size or SHA-256 mismatch")
    endif()
    if(NOT manifest_hash STREQUAL "d753847f9389d5dd07a04d10013f81ffe85c4cfda1dd5120f90eb3e6e46d79af")
        message(FATAL_ERROR "TGMR canonical validation manifest SHA-256 mismatch")
    endif()
    if(NOT notice_hash STREQUAL "67c386e1046e250894c3e7c2867bdb1760211a43bc16c219c8a6bc6e19dbd1ad")
        message(FATAL_ERROR "TGMR corpus attribution SHA-256 mismatch")
    endif()
    file(READ "${TGMR_OFFICIAL_MODEL}" attribution_hash OFFSET 224 LIMIT 32 HEX)
    if(NOT attribution_hash STREQUAL notice_hash)
        message(FATAL_ERROR "TGMR model is not bound to the supplied corpus attribution")
    endif()
    install(FILES "${TGMR_OFFICIAL_MODEL}" DESTINATION "${DATADIR}/models"
        RENAME xtrans-tgmr-v2.tgmr)
    install(FILES "${TGMR_OFFICIAL_MODEL_MANIFEST}" "${TGMR_CORPUS_ATTRIBUTION}"
        "${TGMR_MODEL_LICENSE}" DESTINATION "${DATADIR}/models")
endif()
