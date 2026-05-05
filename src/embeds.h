#pragma once

struct MemoryBlock
{
    const void* data;
    size_t size;
};

MemoryBlock GetTonemapVSBytecode();
MemoryBlock GetTonemapPSBytecode();
MemoryBlock GetImage(int index);
MemoryBlock GetOpenSourceLicensesData();
