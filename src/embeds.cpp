#include "precompiled_header.h"

#include "embeds.h"

#include "image_0.h"
#include "image_1.h"
#include "image_2.h"
#include "image_3.h"
#include "image_4.h"
#include "image_5.h"
#include "open_source_licenses.h"
#include "tonemap_ps.dxbc.h"
#include "tonemap_vs.dxbc.h"

MemoryBlock GetTonemapVSBytecode()
{
    return {
        .data = tonemap_vs_bytecode,
        .size = sizeof(tonemap_vs_bytecode),
    };
}

MemoryBlock GetTonemapPSBytecode()
{
    return {
        .data = tonemap_ps_bytecode,
        .size = sizeof(tonemap_ps_bytecode),
    };
}

MemoryBlock GetImage(int index)
{
    switch (index)
    {
    case 0:
        return {
            .data = image_0,
            .size = sizeof(image_0),
        };
    case 1:
        return {
            .data = image_1,
            .size = sizeof(image_1),
        };
    case 2:
        return {
            .data = image_2,
            .size = sizeof(image_2),
        };
    case 3:
        return {
            .data = image_3,
            .size = sizeof(image_3),
        };
    case 4:
        return {
            .data = image_4,
            .size = sizeof(image_4),
        };
    case 5:
        return {
            .data = image_5,
            .size = sizeof(image_5),
        };
    default:
        assert(0);
        return {};
    }
}

MemoryBlock GetOpenSourceLicensesData()
{
    return {
        .data = open_source_licenses_data,
        .size = sizeof(open_source_licenses_data),
    };
}
