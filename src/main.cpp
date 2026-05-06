#include "precompiled_header.h"

#include "embeds.h"
#include "resource.h"
#include "shaders/cpp_shared.hlsli"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

#define WINDOW_CLASS_NAME L"RealFakeSDRDisplayInHDRWindowClass"
#define WINDOW_TITLE L"Real Fake SDR Display in HDR"

#define WM_LOAD_BUNDLED_IMAGE (WM_USER + 0x0001)

// Current image
uint32_t ImageWidth;
uint32_t ImageHeight;
// RGBA8 format, 4 bytes per pixel
std::vector<std::byte> ImageData;

// Window
constexpr DWORD WindowStyle = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME;
ATOM WindowClass;
HWND WindowHWND;
uint32_t WindowWidth;
uint32_t WindowHeight;

// Render Resources
using Microsoft::WRL::ComPtr;
ComPtr<ID3D11Device> Device;
ComPtr<ID3D11DeviceContext> DeviceContext;
ComPtr<IDXGISwapChain> Swapchain;
ComPtr<IDXGISwapChain3> Swapchain3;
ComPtr<ID3D11RasterizerState> RasterizerState;
ComPtr<ID3D11BlendState> BlendState;
ComPtr<ID3D11DepthStencilState> DepthStencilState;
ComPtr<ID3D11Buffer> ConstantBuffer;

// Resolution dependant resources
ComPtr<ID3D11Texture2D> SwapchainBackbuffer;
ComPtr<ID3D11RenderTargetView> SwapchainBackbufferRTV;
ComPtr<ID3D11Texture2D> SourceImage;
ComPtr<ID3D11ShaderResourceView> SourceImageSRV;
ComPtr<ID3D11Texture2D> ImGuiTarget;
ComPtr<ID3D11RenderTargetView> ImGuiTargetRTV;
ComPtr<ID3D11ShaderResourceView> ImGuiTargetSRV;
ComPtr<ID3D11VertexShader> TonemapVS;
ComPtr<ID3D11PixelShader> TonemapPS;

// Display Parameters
enum OETFType : uint32_t
{
    OETFType_PowerFunction = 0,
    OETFType_sRGBInverseEOTF = 1,
};

// Always using D65 white point
constexpr float WhiteX = 0.3127f;
constexpr float WhiteY = 0.3290f;

struct PrimaryColors
{
    float RedX = 0.68f;
    float RedY = 0.32f;
    float GreenX = 0.265f;
    float GreenY = 0.69f;
    float BlueX = 0.15f;
    float BlueY = 0.06f;
};

PrimaryColors LerpPrimaryColors(const PrimaryColors& a, const PrimaryColors& b, float t)
{
    return PrimaryColors{.RedX = std::lerp(a.RedX, b.RedX, t),
                         .RedY = std::lerp(a.RedY, b.RedY, t),
                         .GreenX = std::lerp(a.GreenX, b.GreenX, t),
                         .GreenY = std::lerp(a.GreenY, b.GreenY, t),
                         .BlueX = std::lerp(a.BlueX, b.BlueX, t),
                         .BlueY = std::lerp(a.BlueY, b.BlueY, t)};
}

float SelectedDisplayLuminanceLevel = 120.0f;
int SelectedOETFType = OETFType_PowerFunction;
// Only used when SelectedOETFType = OETFType_PowerFunction
float SelectedPowerFunctionCharacteristic = 2.2f;

bool SelectedCustomColorGamut = false;
// Only used when SelectedAdvancedColorSettings = false
float SelectedVividness = 0.52f;
// Only used when SelectedAdvancedColorSettings = true
// Defaults - Exact primary colors of authors display (as measured)
PrimaryColors SelectedPrimaryColors = {
    .RedX = 0.690759991f,
    .RedY = 0.302420133f,
    .GreenX = 0.232101794f,
    .GreenY = 0.70252915f,
    .BlueX = 0.154639025f,
    .BlueY = 0.037897682f,
};

constexpr PrimaryColors sRGBPrimaryColors = {
    .RedX = 0.640f,
    .RedY = 0.330f,
    .GreenX = 0.300f,
    .GreenY = 0.600f,
    .BlueX = 0.150f,
    .BlueY = 0.060f,
};

constexpr PrimaryColors Rec2020PrimaryColors = {
    .RedX = 0.708f,
    .RedY = 0.292f,
    .GreenX = 0.170f,
    .GreenY = 0.797f,
    .BlueX = 0.131f,
    .BlueY = 0.046f,
};

DirectX::XMMATRIX GetRGBToXYZMatrix(const PrimaryColors& primaries)
{
    using namespace DirectX;

    float Xr = primaries.RedX / primaries.RedY;
    float Yr = 1.0f;
    float Zr = (1.0f - primaries.RedX - primaries.RedY) / primaries.RedY;

    float Xg = primaries.GreenX / primaries.GreenY;
    float Yg = 1.0f;
    float Zg = (1.0f - primaries.GreenX - primaries.GreenY) / primaries.GreenY;

    float Xb = primaries.BlueX / primaries.BlueY;
    float Yb = 1.0f;
    float Zb = (1.0f - primaries.BlueX - primaries.BlueY) / primaries.BlueY;

    float Xw = WhiteX / WhiteY;
    float Yw = 1.0f;
    float Zw = (1.0f - WhiteX - WhiteY) / WhiteY;

    XMMATRIX primaryMatrix =
        XMMatrixSet(Xr, Yr, Zr, 0.0f, Xg, Yg, Zg, 0.0f, Xb, Yb, Zb, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    XMVECTOR whitePoint = XMVectorSet(Xw, Yw, Zw, 0.0f);

    XMVECTOR determinant;
    XMMATRIX primaryInverse = XMMatrixInverse(&determinant, primaryMatrix);

    XMVECTOR S = XMVector3TransformNormal(whitePoint, primaryInverse);

    XMVECTOR row0 = XMVectorMultiply(XMVectorSplatX(S), primaryMatrix.r[0]);
    XMVECTOR row1 = XMVectorMultiply(XMVectorSplatY(S), primaryMatrix.r[1]);
    XMVECTOR row2 = XMVectorMultiply(XMVectorSplatZ(S), primaryMatrix.r[2]);
    XMVECTOR row3 = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    return XMMATRIX(row0, row1, row2, row3);
}

DirectX::XMMATRIX GenerateColorSpaceConversionMatrix(const PrimaryColors& a, const PrimaryColors& b)
{
    DirectX::XMMATRIX matA = GetRGBToXYZMatrix(a);
    DirectX::XMMATRIX matB = GetRGBToXYZMatrix(b);

    DirectX::XMVECTOR det;
    DirectX::XMMATRIX invMatB = XMMatrixInverse(&det, matB);

    return XMMatrixMultiply(matA, invMatB);
}

void ShowOpenSourceLicenses()
{
    std::filesystem::path tempFilePath =
        std::filesystem::temp_directory_path() / "RealFakeSDRDisplayInHDROpenSourceLicenses.txt";
    {
        std::ofstream tempFileStream(tempFilePath, std::ios::binary);
        MemoryBlock fileData = GetOpenSourceLicensesData();
        tempFileStream.write((const char*)fileData.data, fileData.size);
    }

    std::wstring tempFilePathStr = tempFilePath.wstring();
    ShellExecuteW(nullptr, L"open", tempFilePathStr.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
}

void UpdateWindowSize()
{
    RECT calculatedRect;
    calculatedRect.left = 0;
    calculatedRect.top = 0;
    calculatedRect.right = ImageWidth;
    calculatedRect.bottom = ImageHeight;

    ::AdjustWindowRect(&calculatedRect, WindowStyle, FALSE);

    WindowWidth = calculatedRect.right - calculatedRect.left;
    WindowHeight = calculatedRect.bottom - calculatedRect.top;
}

void CreateResolutionDependantResources()
{
    SwapchainBackbuffer.Reset();
    SwapchainBackbufferRTV.Reset();

    DXGI_MODE_DESC modeDesc = {
        .Width = ImageWidth,
        .Height = ImageHeight,
        .RefreshRate = {},
        .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
    };
    HRESULT hr = Swapchain->ResizeTarget(&modeDesc);
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to resize swapchain target", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    hr = Swapchain->ResizeBuffers(2, ImageWidth, ImageHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, 0);
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to resize swapchain buffers", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    hr = Swapchain->GetBuffer(0, IID_ID3D11Texture2D,
                              (void**)SwapchainBackbuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to get swapchain backbuffer", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                                             .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
                                             .Texture2D = {.MipSlice = 0}};

    hr = Device->CreateRenderTargetView(SwapchainBackbuffer.Get(), &rtvDesc,
                                        SwapchainBackbufferRTV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 swapchain backbuffer RTV", L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {.Width = ImageWidth,
                                        .Height = ImageHeight,
                                        .MipLevels = 1,
                                        .ArraySize = 1,
                                        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                        .SampleDesc = {.Count = 1, .Quality = 0},
                                        .Usage = D3D11_USAGE_DEFAULT,
                                        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                        .CPUAccessFlags = 0,
                                        .MiscFlags = 0};

    D3D11_SUBRESOURCE_DATA resourceData = {
        .pSysMem = ImageData.data(), .SysMemPitch = ImageWidth * 4, .SysMemSlicePitch = 0};

    hr = Device->CreateTexture2D(&textureDesc, &resourceData, SourceImage.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 source image texture", L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                               .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                                               .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}};
    hr = Device->CreateShaderResourceView(SourceImage.Get(), &srvDesc,
                                          SourceImageSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 source image texture SRV", L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    hr = Device->CreateTexture2D(&textureDesc, NULL, ImGuiTarget.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 ImGui target texture", L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    hr = Device->CreateRenderTargetView(ImGuiTarget.Get(), &rtvDesc,
                                        ImGuiTargetRTV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 ImGui target RTV", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    hr = Device->CreateShaderResourceView(ImGuiTarget.Get(), &srvDesc,
                                          ImGuiTargetSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 ImGui target SRV", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }
}

DirectX::XMMATRIX GetVibranceCorrectionMatrix()
{
    PrimaryColors source =
        SelectedCustomColorGamut
            ? SelectedPrimaryColors
            : LerpPrimaryColors(sRGBPrimaryColors, Rec2020PrimaryColors, SelectedVividness);
    return GenerateColorSpaceConversionMatrix(source, sRGBPrimaryColors);
}

void UpdateConstantBuffer()
{
    ShaderTypes::ConversionParametersStruct conversionParameters = {
        .DisplayLuminanceLevel = SelectedDisplayLuminanceLevel / 80.0f,
        .OETFType = static_cast<uint32_t>(SelectedOETFType),
        .PowerFunctionCharacteristic = SelectedPowerFunctionCharacteristic,
        .ColorTransform = GetVibranceCorrectionMatrix()};

    DeviceContext->UpdateSubresource(ConstantBuffer.Get(), 0, NULL, &conversionParameters, 0, 0);
}

void RenderImGui()
{
    // ImGuiWindowFlags_AlwaysAutoResize has 1 frame delay
    // Hide it from user by re-rendering (only ImGui, not d3d11 part) upon change of any parameter
    // that affects layour of the window
    bool needReRender = false;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::Begin("Switches", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Image selection");

    if (ImGui::Button("Title"))
    {
        PostMessage(WindowHWND, WM_LOAD_BUNDLED_IMAGE, 0, 0);
    }

    for (int i = 1; i <= 5; ++i)
    {
        ImGui::SameLine();
        std::string indexStr = std::to_string(i);
        if (ImGui::Button(indexStr.c_str()))
        {
            PostMessage(WindowHWND, WM_LOAD_BUNDLED_IMAGE, i, 0);
        }
    }

    ImGui::Text("Presets");

    if (ImGui::Button("Vivid"))
    {
        needReRender = true;
        SelectedOETFType = OETFType_PowerFunction;
        SelectedPowerFunctionCharacteristic = 2.2f;
        SelectedCustomColorGamut = false;
        SelectedVividness = 0.52f;
    }

    ImGui::SameLine();
    if (ImGui::Button("Balanced"))
    {
        needReRender = true;
        SelectedOETFType = OETFType_PowerFunction;
        SelectedPowerFunctionCharacteristic = 2.2f;
        SelectedCustomColorGamut = false;
        SelectedVividness = 0.26f;
    }

    if (ImGui::Button("sRGB Reference Display"))
    {
        needReRender = true;
        SelectedOETFType = OETFType_PowerFunction;
        SelectedPowerFunctionCharacteristic = 2.2f;
        SelectedCustomColorGamut = false;
        SelectedVividness = 0.0f;
    }

    if (ImGui::Button("Windows Conversion"))
    {
        needReRender = true;
        SelectedOETFType = OETFType_sRGBInverseEOTF;
        SelectedPowerFunctionCharacteristic = 2.2f;
        SelectedCustomColorGamut = false;
        SelectedVividness = 0.0f;
    }

    ImGui::Spacing();

    if (ImGui::Button("Show Open Source Licenses"))
    {
        ShowOpenSourceLicenses();
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(350, 20), ImGuiCond_FirstUseEver);

    ImGui::Begin("Display Parameters", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGui::SliderFloat("Display Luminance Level", &SelectedDisplayLuminanceLevel, 80.0f, 480.0f,
                       NULL, ImGuiSliderFlags_AlwaysClamp);

    needReRender |= ImGui::RadioButton("Use Power Function OETF", &SelectedOETFType, 0);
    needReRender |= ImGui::RadioButton("Use sRGB inverse EOTF as OETF", &SelectedOETFType, 1);

    if (SelectedOETFType == OETFType_PowerFunction)
    {
        ImGui::SliderFloat("Power function characteristic", &SelectedPowerFunctionCharacteristic,
                           1.0f, 3.0f, NULL, ImGuiSliderFlags_AlwaysClamp);
    }

    needReRender |= ImGui::Checkbox("Custom Color Gamut", &SelectedCustomColorGamut);

    if (!SelectedCustomColorGamut)
    {
        ImGui::SliderFloat("Vividness", &SelectedVividness, 0.0f, 1.0f, NULL,
                           ImGuiSliderFlags_AlwaysClamp);
    }
    else
    {
        ImGui::InputFloat("Red x  ", &SelectedPrimaryColors.RedX, 0.0f, 1.0f, NULL);
        ImGui::SameLine();
        ImGui::InputFloat("Red y  ", &SelectedPrimaryColors.RedY, 0.0f, 1.0f, NULL);
        ImGui::InputFloat("Green x", &SelectedPrimaryColors.GreenX, 0.0f, 1.0f, NULL);
        ImGui::SameLine();
        ImGui::InputFloat("Green y", &SelectedPrimaryColors.GreenY, 0.0f, 1.0f, NULL);
        ImGui::InputFloat("Blue x ", &SelectedPrimaryColors.BlueX, 0.0f, 1.0f, NULL);
        ImGui::SameLine();
        ImGui::InputFloat("Blue y ", &SelectedPrimaryColors.BlueY, 0.0f, 1.0f, NULL);
    }

    ImGui::End();

    if (needReRender)
    {
        ImGui::EndFrame();
        RenderImGui();
    }
    else
    {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

void RenderFrame()
{
    UpdateConstantBuffer();

    ID3D11RenderTargetView* nullRTVToSet[] = {nullptr};
    ID3D11RenderTargetView* imguiRTVToSet[] = {ImGuiTargetRTV.Get()};
    ID3D11RenderTargetView* swapchainRTVToSet[] = {SwapchainBackbufferRTV.Get()};

    ID3D11ShaderResourceView* nullSRVToSet[] = {nullptr, nullptr};
    ID3D11ShaderResourceView* srvToSet[] = {SourceImageSRV.Get(), ImGuiTargetSRV.Get()};

    ID3D11Buffer* nullCBVToSet[] = {nullptr};
    ID3D11Buffer* cbvToSet[] = {ConstantBuffer.Get()};

    DeviceContext->RSSetState(RasterizerState.Get());
    DeviceContext->OMSetBlendState(BlendState.Get(), NULL, 0xffffffff);
    DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);

    D3D11_VIEWPORT viewportDesc = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(ImageWidth),
        .Height = static_cast<float>(ImageHeight),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    DeviceContext->RSSetViewports(1, &viewportDesc);

    DeviceContext->OMSetRenderTargets(1, imguiRTVToSet, NULL);

    FLOAT clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    DeviceContext->ClearRenderTargetView(ImGuiTargetRTV.Get(), clearColor);

    RenderImGui();

    DeviceContext->OMSetRenderTargets(1, nullRTVToSet, NULL);

    DeviceContext->VSSetShader(TonemapVS.Get(), NULL, 0);
    DeviceContext->PSSetShader(TonemapPS.Get(), NULL, 0);

    DeviceContext->PSSetConstantBuffers(0, 1, cbvToSet);

    DeviceContext->PSSetShaderResources(0, 2, srvToSet);

    DeviceContext->OMSetRenderTargets(1, swapchainRTVToSet, NULL);

    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    DeviceContext->Draw(6, 0);

    DeviceContext->PSSetConstantBuffers(0, 1, nullCBVToSet);
    DeviceContext->PSSetShaderResources(0, 2, nullSRVToSet);
    DeviceContext->OMSetRenderTargets(1, nullRTVToSet, NULL);

    HRESULT hr = Swapchain->Present(1, 0);
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Swapchain presentation failed", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }
}

void LoadImageFromFile(WPARAM wParam)
{
    // Here we cast the wParam as a HDROP handle to pass into the next functions
    HDROP hDrop = (HDROP)wParam;

    // This returns number of files
    int count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

    if (count < 1)
    {
        MessageBoxW(0, L"You somehow dropped less than 1 file", L"Error", MB_OK | MB_ICONERROR);
        DragFinish(hDrop);
        return;
    }

    if (count > 1)
    {
        MessageBoxW(0, L"You can only drop 1 file at a time", L"Error", MB_OK | MB_ICONERROR);
        DragFinish(hDrop);
        return;
    }

    int pathLength = DragQueryFile(hDrop, 0, NULL, 0);

    std::string filePath;
    filePath.resize(pathLength + 1);

    int res = DragQueryFile(hDrop, 0, filePath.data(), pathLength + 1);

    DragFinish(hDrop);

    if (res == 0)
    {
        MessageBoxW(0, L"Failed to get file path", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    int width;
    int height;

    stbi_uc* image = stbi_load(filePath.c_str(), &width, &height, nullptr, 4);
    if (image == nullptr)
    {
        MessageBoxW(0, L"Failed to load image. Please make sure you are dropping an image file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    ImageWidth = static_cast<uint32_t>(width);
    ImageHeight = static_cast<uint32_t>(height);

    ImageData.resize(ImageWidth * ImageHeight * 4);
    memcpy(ImageData.data(), image, ImageData.size());
    stbi_image_free(image);
    UpdateWindowSize();

    CreateResolutionDependantResources();
}

void LoadImageFromClipboard()
{
    BOOL res = OpenClipboard(WindowHWND);
    if (!res)
    {
        MessageBoxW(0, L"Failed to open clipboard", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    HANDLE hData = GetClipboardData(CF_BITMAP);
    if (hData == NULL)
    {
        MessageBoxW(0, L"Failed to get image from clipboard", L"Error", MB_OK | MB_ICONERROR);
        CloseClipboard();
        return;
    }
    HBITMAP hBitmap = static_cast<HBITMAP>(hData);

    BITMAP bitmap;
    if (GetObject(hBitmap, sizeof(bitmap), &bitmap) == 0)
    {
        MessageBoxW(0, L"Failed to read bitmap info", L"Error", MB_OK | MB_ICONERROR);
        CloseClipboard();
        return;
    }

    uint32_t newWidth = bitmap.bmWidth;
    uint32_t newHeight = bitmap.bmHeight;

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = newWidth;
    // Negative height tells Windows we want a Top-Down image.
    // Positive height would result in an upside-down image.
    bitmapInfo.bmiHeader.biHeight = -static_cast<int>(newHeight);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    std::vector<std::byte> newImageData;
    newImageData.resize(newWidth * newHeight * 4);

    HDC hdc = GetDC(WindowHWND);
    int scanlines =
        GetDIBits(hdc, hBitmap, 0, newHeight, newImageData.data(), &bitmapInfo, DIB_RGB_COLORS);
    ReleaseDC(WindowHWND, hdc);

    if (scanlines == 0)
    {
        MessageBoxW(0, L"Failed to extract pixels", L"Error", MB_OK | MB_ICONERROR);
        CloseClipboard();
        return;
    }

    // Flip BGRA into RGBA
    for (size_t i = 0; i < newImageData.size(); i += 4)
    {
        std::byte b = newImageData[i];
        std::byte r = newImageData[i + 2];

        newImageData[i] = r;
        newImageData[i + 2] = b;
        newImageData[i + 3] = static_cast<std::byte>(255);
    }

    ImageWidth = newWidth;
    ImageHeight = newHeight;
    ImageData = std::move(newImageData);
    UpdateWindowSize();

    CloseClipboard();
    CreateResolutionDependantResources();
}

void LoadImageFromMemory(MemoryBlock imageBytes)
{
    int width;
    int height;

    stbi_uc* image = stbi_load_from_memory(static_cast<const stbi_uc*>(imageBytes.data),
                                           imageBytes.size, &width, &height, nullptr, 4);
    if (image == nullptr)
    {
        MessageBoxW(0, L"Failed to load built-in image", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    ImageWidth = static_cast<uint32_t>(width);
    ImageHeight = static_cast<uint32_t>(height);

    ImageData.resize(ImageWidth * ImageHeight * 4);
    memcpy(ImageData.data(), image, ImageData.size());
    stbi_image_free(image);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
    {
        InvalidateRect(hwnd, NULL, FALSE);
        return true;
    }

    switch (message)
    {
    case WM_PAINT: {
        // When recursive WM_PAINT occurs, it is both wasteful and breaks our internal logic
        // Make sure that we skip the message if we are already handling it up the callstack
        static bool isPainting = false;
        if (!isPainting)
        {
            isPainting = true;
            RenderFrame();
            ValidateRect(hwnd, NULL);
            isPainting = false;
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = WindowWidth;
        lpMMI->ptMinTrackSize.y = WindowHeight;
        lpMMI->ptMaxTrackSize.x = WindowWidth;
        lpMMI->ptMaxTrackSize.y = WindowHeight;
        return 0;
    }
    case WM_SIZING: {
        RECT* rect = (RECT*)lParam;
        rect->right = rect->left + WindowWidth;
        rect->bottom = rect->top + WindowHeight;
        return 0;
    }
    case WM_NCDESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_PASTE:
            LoadImageFromClipboard();
            return 0;
        }
        return 0;
    }
    case WM_DROPFILES:
        LoadImageFromFile(wParam);
        return 0;
    case WM_LOAD_BUNDLED_IMAGE:
        LoadImageFromMemory(GetImage((int)wParam));
        UpdateWindowSize();
        CreateResolutionDependantResources();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    // Events that are handled by ImGui
    // By redrawing upon any of those, we ensure that we always redraw
    // if ImGui's state ever changes due to window message
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
    case WM_DESTROY:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_INPUTLANGCHANGE:
    case WM_CHAR:
    case WM_IME_COMPOSITION:
    case WM_IME_CHAR:
    case WM_SETCURSOR:
    case WM_DEVICECHANGE:
        InvalidateRect(hwnd, NULL, FALSE);
        [[fallthrough]];
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW windowClassDesc = {};
    windowClassDesc.cbSize = sizeof(windowClassDesc);
    windowClassDesc.style = CS_HREDRAW | CS_VREDRAW;
    windowClassDesc.lpfnWndProc = &WindowProc;
    windowClassDesc.hInstance = hInstance;
    windowClassDesc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    windowClassDesc.lpszClassName = WINDOW_CLASS_NAME;

    WindowClass = RegisterClassExW(&windowClassDesc);

    if (WindowClass == 0)
    {
        MessageBoxW(0, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    UpdateWindowSize();

    WindowHWND = CreateWindowExW(WS_EX_ACCEPTFILES, WINDOW_CLASS_NAME, WINDOW_TITLE, WindowStyle,
                                 CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight, NULL,
                                 NULL, hInstance, NULL);

    if (WindowHWND == NULL)
    {
        MessageBoxW(0, L"Failed to create main window", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }
}

void InitializeDeviceAndSwapchain()
{
    DXGI_SWAP_CHAIN_DESC swapchainDesc = {
        .BufferDesc =
            {
                .Width = ImageWidth,
                .Height = ImageHeight,
                .RefreshRate = {.Numerator = 60, .Denominator = 1},
                .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            },
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = WindowHWND,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = 0,
    };

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, NULL, 0, D3D11_SDK_VERSION,
        &swapchainDesc, Swapchain.ReleaseAndGetAddressOf(), Device.ReleaseAndGetAddressOf(), NULL,
        DeviceContext.ReleaseAndGetAddressOf());

    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 device and swapchain", L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    hr = Swapchain.As(&Swapchain3);

    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to query IDXGISwapChain3", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    hr = Swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
}

void InitializeRendering()
{

    D3D11_RASTERIZER_DESC rasterizerDesc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .FrontCounterClockwise = FALSE,
        .DepthBias = 0,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = 0.0f,
        .DepthClipEnable = FALSE,
        .ScissorEnable = FALSE,
        .MultisampleEnable = FALSE,
        .AntialiasedLineEnable = FALSE,
    };

    HRESULT hr =
        Device->CreateRasterizerState(&rasterizerDesc, RasterizerState.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 rasterizer state", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    D3D11_BLEND_DESC blendDesc = {
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
        .RenderTarget = {{.BlendEnable = FALSE,
                          .SrcBlend = D3D11_BLEND_ONE,
                          .DestBlend = D3D11_BLEND_ZERO,
                          .BlendOp = D3D11_BLEND_OP_ADD,
                          .SrcBlendAlpha = D3D11_BLEND_ONE,
                          .DestBlendAlpha = D3D11_BLEND_ZERO,
                          .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                          .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL}},
    };

    hr = Device->CreateBlendState(&blendDesc, BlendState.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 blend state", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {
        .DepthEnable = FALSE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc = D3D11_COMPARISON_ALWAYS,
        .StencilEnable = FALSE,
        .StencilReadMask = 0,
        .StencilWriteMask = 0,
        .FrontFace = {},
        .BackFace = {},
    };

    hr = Device->CreateDepthStencilState(&depthStencilDesc,
                                         DepthStencilState.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 depth stencil state", L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    MemoryBlock tonemapVSBytecode = GetTonemapVSBytecode();
    hr = Device->CreateVertexShader(tonemapVSBytecode.data, tonemapVSBytecode.size, NULL,
                                    TonemapVS.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 vertex shader", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    MemoryBlock tonemapPSBytecode = GetTonemapPSBytecode();
    hr = Device->CreatePixelShader(tonemapPSBytecode.data, tonemapPSBytecode.size, NULL,
                                   TonemapPS.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 pixel shader", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    D3D11_BUFFER_DESC bufferDesc = {
        .ByteWidth = sizeof(ShaderTypes::ConversionParametersStruct),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0,
    };

    hr = Device->CreateBuffer(&bufferDesc, NULL, ConstantBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create D3D11 constant buffer", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    CreateResolutionDependantResources();
}

void ShowWindow()
{
    ::ShowWindow(WindowHWND, SW_SHOW);
    ::UpdateWindow(WindowHWND);
}

void MessageLoop(HINSTANCE hInstance)
{
    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        if (!TranslateAccelerator(WindowHWND, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

void CheckHDRSupport()
{
    bool isHDREnabled = false;

    ComPtr<IDXGIFactory1> dxgiFactory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to create DXGI Factory", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; SUCCEEDED(dxgiFactory->EnumAdapters1(adapterIndex, &adapter));
         ++adapterIndex)
    {
        ComPtr<IDXGIOutput> output;
        for (UINT outputIndex = 0; SUCCEEDED(adapter->EnumOutputs(outputIndex, &output));
             ++outputIndex)
        {
            ComPtr<IDXGIOutput6> output6;

            hr = output.As(&output6);
            if (FAILED(hr))
            {
                MessageBoxW(0, L"Failed to query IDXGIOutput6", L"Error", MB_OK | MB_ICONERROR);
                ExitProcess(1);
                return;
            }

            DXGI_OUTPUT_DESC1 desc1;
            hr = output6->GetDesc1(&desc1);
            if (FAILED(hr))
            {
                MessageBoxW(0, L"Failed to query output desc", L"Error", MB_OK | MB_ICONERROR);
                ExitProcess(1);
                return;
            }

            if (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
            {
                isHDREnabled = true;
                break;
            }
        }

        if (isHDREnabled)
        {
            break;
        }
    }

    if (isHDREnabled)
    {
        return;
    }

    MessageBoxW(nullptr,
                L"HDR is not enabled or not supported on any of your displays.\n\nPlease enable "
                L"HDR in your Windows Display Settings and re-launch the app.",
                L"HDR Support Required", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);

    ShellExecuteW(nullptr, L"open", L"ms-settings:display", nullptr, nullptr, SW_SHOWDEFAULT);
    ExitProcess(1);
    return;
}

void InitializeImGui()
{
    IMGUI_CHECKVERSION();

    ImGui_ImplWin32_EnableDpiAwareness();
    float scalingFactor = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scalingFactor);
    style.FontScaleDpi = scalingFactor;
    style.DisplayWindowPadding = ImVec2(200.0f, 200.0f);

    ImGui_ImplWin32_Init(WindowHWND);
    ImGui_ImplDX11_Init(Device.Get(), DeviceContext.Get());
}

void CleanupImGui()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
}

void CleanupRendering()
{
    Swapchain.Reset();
    Swapchain3.Reset();
    RasterizerState.Reset();
    BlendState.Reset();
    DepthStencilState.Reset();
    ConstantBuffer.Reset();
    SwapchainBackbuffer.Reset();
    SwapchainBackbufferRTV.Reset();
    SourceImage.Reset();
    SourceImageSRV.Reset();
    ImGuiTarget.Reset();
    ImGuiTargetRTV.Reset();
    ImGuiTargetSRV.Reset();
    TonemapVS.Reset();
    TonemapPS.Reset();
    DeviceContext.Reset();
    Device.Reset();
}

void InitializeWin32()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        MessageBoxW(0, L"Failed to initialize COM", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
        return;
    }
}

void CleanupWin32()
{
    CoUninitialize();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    InitializeWin32();
#ifndef _DEBUG
    CheckHDRSupport();
#endif
    LoadImageFromMemory(GetImage(0));
    CreateMainWindow(hInstance);
    InitializeDeviceAndSwapchain();
    InitializeRendering();
    InitializeImGui();
    RenderFrame();
    ShowWindow();
    MessageLoop(hInstance);
    CleanupImGui();
    CleanupRendering();
    CleanupWin32();
}
