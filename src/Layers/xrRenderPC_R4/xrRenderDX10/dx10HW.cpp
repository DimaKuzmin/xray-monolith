// dx10HW.cpp: implementation of the DX10 specialisation of CHW.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#pragma hdrstop

#pragma warning(disable:4995)
#include <d3dx9.h>
#ifdef USE_DX11
# include <d3d11_4.h>
# include <dxgi1_5.h>
#endif
#pragma warning(default:4995)
#include "HW.h"
#include "../../xrEngine/XR_IOConsole.h"
#include "../../Include/xrAPI/xrAPI.h"
#include "xrRender_console.h"

#include "StateManager\dx10SamplerStateCache.h"
#include "StateManager\dx10StateCache.h"

 
void fill_vid_mode_list(CHW* _hw);
void free_vid_mode_list();

void fill_render_mode_list();
void free_render_mode_list();
 
CHW HW;

//	DX10: Don't neeed this?

LPCSTR dxgiOld = "--dxgi-old";

CHW::CHW() :
	m_pAdapter(0),
	pDevice(NULL),
	m_move_window(true)
{
    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this);
}

CHW::~CHW()
{
    Device.seqAppActivate.Remove(this);
    Device.seqAppDeactivate.Remove(this);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
void CHW::CreateD3D()
{
 
    R_CHK(CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pFactory)));
 
    m_pAdapter    = 0;
    m_bUsePerfhud = false;
 
    if (!m_pAdapter) 
         m_pFactory->EnumAdapters1(0, &m_pAdapter);
 
#if defined(USE_DX11)
    // when using FLIP_* present modes, to disable DWM vsync we have to use DXGI_PRESENT_ALLOW_TEARING with ->Present()
    // when vsync is off (PresentInterval = 0) and only when in window mode
    // store whether we can use the flag for later use (swapchain creation, buffer resize, present call)

    // TODO: On some PC configurations (versions of Windows, graphics drivers, currently unknown what exactly) this isn't
    // sufficient to disable the DWM vsync when the game is launched in a windowed mode, however if the user switches from
    // borderless/windowed -> exclusive fullscreen -> borderless/windowed then it seems to work correctly.
    // Worth investigating why this is occurring
    //
    // Configuration where this occurs:
    // - Windows 10 Enterprise LTSC, 21H2, 19044.4894
    // - NVIDIA driver version 560.94
    {
        HRESULT hr;

        IDXGIFactory5* factory5 = nullptr;
        hr = m_pFactory->QueryInterface(&factory5);

        if (SUCCEEDED(hr) && factory5) {
            BOOL supports_vrr = FALSE;
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supports_vrr, sizeof(supports_vrr));

            m_SupportsVRR = (SUCCEEDED(hr) && supports_vrr);

            factory5->Release();
        } else {
            m_SupportsVRR = false;
        }
    }
#endif
}

void CHW::DestroyD3D()
{
    _SHOW_REF("refCount:m_pAdapter", m_pAdapter);
    _RELEASE(m_pAdapter);

#if defined(USE_DX11)
    _SHOW_REF("refCount:m_pFactory", m_pFactory);
    _RELEASE(m_pFactory);
#endif
}

extern u32 g_screenmode;

void CHW::CreateDevice(HWND hwnd, bool move_window)
{
    m_hWnd = hwnd;
    m_move_window = move_window;
    CreateD3D();

    // TODO: DX10: Create appropriate initialization

    // General - select adapter and device
    BOOL bWindowed = (g_screenmode != 2);

    m_DriverType = Caps.bForceGPU_REF ? D3D_DRIVER_TYPE_REFERENCE : D3D_DRIVER_TYPE_HARDWARE;

    if (m_bUsePerfhud)
        m_DriverType = D3D_DRIVER_TYPE_REFERENCE;

    // Display the name of video board
    DXGI_ADAPTER_DESC Desc;
    R_CHK(m_pAdapter->GetDesc(&Desc));
    //	Warning: Desc.Description is wide string
    Msg("* GPU [vendor:%X]-[device:%X]: %S", Desc.VendorId, Desc.DeviceId, Desc.Description);

    Caps.id_vendor = Desc.VendorId;
    Caps.id_device = Desc.DeviceId;

    // Select back-buffer & depth-stencil format
    D3DFORMAT& fTarget = Caps.fTarget;
	D3DFORMAT& fDepth = Caps.fDepth;

    //	HACK: DX10: Embed hard target format.
    fTarget = D3DFMT_X8R8G8B8; //	No match in DX10. D3DFMT_A8B8G8R8->DXGI_FORMAT_R8G8B8A8_UNORM
	fDepth = selectDepthStencil(fTarget);
 
    if ((D3DFMT_UNKNOWN==fTarget) || (D3DFMT_UNKNOWN==fTarget))	{
		Msg					("Failed to initialize graphics hardware.\nPlease try to restart the game.");
		FlushLog			();
		MessageBox			(NULL,"Failed to initialize graphics hardware.\nPlease try to restart the game.","Error!",MB_OK|MB_ICONERROR);
		TerminateProcess	(GetCurrentProcess(),0);
    }


    // Set up the presentation parameters
    DXGI_SWAP_CHAIN_DESC1& sd = m_ChainDesc;
    ZeroMemory(&sd, sizeof(sd));

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC& sd_fullscreen = m_ChainDescFullscreen;
    ZeroMemory(&sd_fullscreen, sizeof(sd_fullscreen));
 
    selectResolution(sd.Width, sd.Height, bWindowed);
    sd_fullscreen.Windowed = bWindowed;
 
    sd.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
    sd.Format      = ps_r4_hdr10_on ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferCount = 2;

    // Multisample
	sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;

    // Required for HDR, provides better performance in windowed/borderless mode
    // https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    if (bWindowed)
    {
        // TODO: fix this, shouldn't just default to 60hz
        sd_fullscreen.RefreshRate.Numerator   = 60;
        sd_fullscreen.RefreshRate.Denominator = 1;
    }
    else
    {
		sd_fullscreen.RefreshRate = selectRefresh(sd.Width, sd.Height, sd.Format);
    }

    //	Additional set up
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

#if defined(USE_DX11)
    if (m_SupportsVRR) {
        sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
#endif

    UINT createDeviceFlags = 0;
    HRESULT R;

    D3D_FEATURE_LEVEL pFeatureLevels[] = {
         D3D_FEATURE_LEVEL_11_0,
    };

    UINT create_device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (strstr(Core.Params, "--dxgi-dbg")) 
    {
        // enables d3d11 debug layer validation and output
        // viewable in VS debugger `Output > Debug` view or using a tool like Sysinternals DebugView
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    // create device
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    R_CHK(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_device_flags,
        pFeatureLevels,
        1,
        D3D11_SDK_VERSION,
        &device,
        &FeatureLevel,
        &context));

    R_CHK(device->QueryInterface(&pDevice));
    R_CHK(context->QueryInterface(&pContext));

    _RELEASE(device);
    _RELEASE(context);

    // create swapchain
    R_CHK(m_pFactory->CreateSwapChainForHwnd(pDevice, m_hWnd, &sd, &sd_fullscreen, NULL, &m_pSwapChain));

    // setup colorspace
    // HDR10 (U10 output) -> DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
    // SDR   (U8 output)  -> DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
    // TODO: SDR 10-bit?
    IDXGISwapChain3* swapchain3;
    R_CHK(m_pSwapChain->QueryInterface(&swapchain3));

    if (ps_r4_hdr10_on) {
        UINT color_space_supported = 0;
        R_CHK(swapchain3->CheckColorSpaceSupport(
            DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,
            &color_space_supported));

        if (color_space_supported & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) {
            R_CHK(swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));
        } else {
            Log("HDR10 color space unsupported, failed to enable HDR10 output");
            R_CHK(swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
        }
    } else {
        R_CHK(swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
    }

    _RELEASE(swapchain3);

	//if (D3DERR_DEVICELOST==R)	{
	if (FAILED(R))
	{
        // Fatal error! Cannot create rendering device AT STARTUP !!!
        Msg("Failed to initialize graphics hardware.\n"
            "Please try to restart the game.\n"
		    "CreateDevice returned 0x%08x", R
		);
        FlushLog();
		MessageBox(NULL, "Failed to initialize graphics hardware.\nPlease try to restart the game.", "Error!",
            MB_OK | MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(), 0);
    };
    R_CHK(R);

    _SHOW_REF("* CREATE: DeviceREF:", HW.pDevice);
   
    // NOTE: this seems required to get the default render target to match the swap chain resolution
    // probably the sequence ResizeTarget, ResizeBuffers, and UpdateViews is important
    
    // u32	memory									= pDevice->GetAvailableTextureMem	();
    if (strstr(Core.Params, dxgiOld)) {
        Msg("* %s enabled", dxgiOld);
        UpdateViews();
        size_t memory = Desc.DedicatedVideoMemory;
        Msg("*     Texture memory: %d M", memory / (1024 * 1024));

        updateWindowProps(hwnd);
        fill_vid_mode_list(this);

    } 
    else 
    {
        size_t memory = Desc.DedicatedVideoMemory;
        Msg("*     Texture memory: %d M", memory / (1024 * 1024));
        Reset(hwnd);
        fill_vid_mode_list(this);
    }
}

void CHW::DestroyDevice()
{
    //	Destroy state managers
    StateManager.Reset();
    RSManager.ClearStateArray();
    DSSManager.ClearStateArray();
    BSManager.ClearStateArray();
    SSManager.ClearStateArray();

    _SHOW_REF("refCount:pBaseZB", pBaseZB);
    _RELEASE(pBaseZB);

    _SHOW_REF("refCount:pBaseRT", pBaseRT);
    _RELEASE(pBaseRT);

    //	Must switch to windowed mode to release swap chain
    BOOL is_windowed = m_ChainDescFullscreen.Windowed;
    if (!is_windowed)
    {
        m_pSwapChain->SetFullscreenState(FALSE, NULL);

        if (strstr(Core.Params, dxgiOld)) 
        {
            const auto& cd = m_ChainDesc;
            CHK_DX(m_pSwapChain->ResizeBuffers(
                cd.BufferCount,
                cd.Width,
                cd.Height,
                cd.Format,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

            UpdateViews();
		}
    }
    _SHOW_REF("refCount:m_pSwapChain", m_pSwapChain);
    _RELEASE(m_pSwapChain);

    _RELEASE(pContext);
    _SHOW_REF("DeviceREF:", HW.pDevice);
    _RELEASE(HW.pDevice);

    DestroyD3D();
    free_vid_mode_list();
}

//////////////////////////////////////////////////////////////////////
// Resetting device
//////////////////////////////////////////////////////////////////////
void CHW::Reset(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC1&           cd    = m_ChainDesc;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC& cd_fs = m_ChainDescFullscreen;

    BOOL bWindowed = (g_screenmode != 2);
    cd_fs.Windowed = bWindowed;
    m_pSwapChain->SetFullscreenState(!bWindowed, NULL);
    selectResolution(cd.Width, cd.Height, bWindowed);


	if (bWindowed)
	{
        // TODO: fix
		cd_fs.RefreshRate.Numerator = 60;
        cd_fs.RefreshRate.Denominator = 1;
	}
	else
    {
        cd_fs.RefreshRate = selectRefresh(cd.Width, cd.Height, cd.Format);
    }

    DXGI_MODE_DESC mode;
    ZeroMemory(&mode, sizeof(mode));

    mode.Width       = cd.Width;
    mode.Height      = cd.Height;
    mode.Format      = cd.Format;
    mode.RefreshRate = cd_fs.RefreshRate;
    CHK_DX(m_pSwapChain->ResizeTarget(&mode));

    _SHOW_REF("refCount:pBaseZB", pBaseZB);
    _SHOW_REF("refCount:pBaseRT", pBaseRT);

    _RELEASE(pBaseZB);
    _RELEASE(pBaseRT);

    UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (m_SupportsVRR) {
        flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    CHK_DX(m_pSwapChain->ResizeBuffers(
        cd.BufferCount,
        cd.Width,
        cd.Height,
        cd.Format,
        flags));

    UpdateViews();
    updateWindowProps(hwnd);
}

D3DFORMAT CHW::selectDepthStencil(D3DFORMAT fTarget)
{
    // R3 hack
#pragma todo("R3 need to specify depth format")
    return D3DFMT_D24S8;
}

extern void GetMonitorResolution(u32& horizontal, u32& vertical);

void CHW::selectResolution(u32& dwWidth, u32& dwHeight, BOOL bWindowed)
{
    fill_vid_mode_list(this);

    if (psCurrentVidMode[0] == 0 || psCurrentVidMode[1] == 0)
        GetMonitorResolution(psCurrentVidMode[0], psCurrentVidMode[1]);

	if (bWindowed)
	{
		dwWidth = psCurrentVidMode[0];
        dwHeight = psCurrentVidMode[1];
	}
	else //check
    {
        string64 buff;
        xr_sprintf(buff, sizeof(buff), "%dx%d", psCurrentVidMode[0], psCurrentVidMode[1]);

		if (_ParseItem(buff, vid_mode_token) == u32(-1)) //not found
        {
			//select safe
            xr_sprintf(buff, sizeof(buff), "vid_mode %s", vid_mode_token[0].name);
            Console->Execute(buff);
        }

		dwWidth = psCurrentVidMode[0];
        dwHeight = psCurrentVidMode[1];
    }
}

//	TODO: DX10: check if we need these
DXGI_RATIONAL CHW::selectRefresh(u32 dwWidth, u32 dwHeight, DXGI_FORMAT fmt)
{
    DXGI_RATIONAL res;

	res.Numerator = 60;
    res.Denominator = 1;

    float CurrentFreq = 60.0f;

	if (psDeviceFlags.is(rsRefresh60hz) || strstr(Core.Params, "-60hz"))
	{
        refresh_rate = 1.f / 60.f;
        return res;
    }

    xr_vector<DXGI_MODE_DESC> modes;

    IDXGIOutput* pOutput;
    m_pAdapter->EnumOutputs(0, &pOutput);
    VERIFY(pOutput);

	UINT num = 0;
    DXGI_FORMAT format = fmt;
	UINT flags = 0;

    // Get the number of display modes available
    pOutput->GetDisplayModeList(format, flags, &num, 0);

    // Get the list of display modes
    modes.resize(num);
    pOutput->GetDisplayModeList(format, flags, &num, &modes.front());

    _RELEASE(pOutput);

	for (u32 i = 0; i < num; ++i)
	{
        DXGI_MODE_DESC& desc = modes[i];

		if ((desc.Width == dwWidth)
			&& (desc.Height == dwHeight)
			)
		{
            VERIFY(desc.RefreshRate.Denominator);
			float TempFreq = float(desc.RefreshRate.Numerator) / float(desc.RefreshRate.Denominator);
			if (TempFreq > CurrentFreq)
			{
                CurrentFreq = TempFreq;
				res = desc.RefreshRate;
            }
        }
    }

    refresh_rate = 1.f / CurrentFreq;

    return res;
}

extern bool use_reshade;
extern bool init_reshade();
extern void unregister_reshade();

void CHW::OnAppActivate()
{
    BOOL is_windowed = m_ChainDescFullscreen.Windowed;
	if (m_pSwapChain && !is_windowed)
	{
        ShowWindow(m_hWnd, SW_RESTORE);
        m_pSwapChain->SetFullscreenState(TRUE, NULL);


        if (!strstr(Core.Params, dxgiOld)) {
            _SHOW_REF("refCount:pBaseZB", pBaseZB);
            _RELEASE(pBaseZB);

            _SHOW_REF("refCount:pBaseRT", pBaseRT);
            _RELEASE(pBaseRT);
        }

        const auto& cd = m_ChainDesc;

        UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (m_SupportsVRR) {
            flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        m_pSwapChain->ResizeBuffers(
            cd.BufferCount,
            cd.Width,
            cd.Height,
            cd.Format,
            flags);

        UpdateViews();

		if (use_reshade)
            init_reshade();
    }
}

void CHW::OnAppDeactivate()
{
    BOOL is_windowed = m_ChainDescFullscreen.Windowed;
	if (m_pSwapChain && !is_windowed)
	{
		if (use_reshade)
            unregister_reshade();
		
        m_pSwapChain->SetFullscreenState(FALSE, NULL);

        if (!strstr(Core.Params, dxgiOld)) {
            _SHOW_REF("refCount:pBaseZB", pBaseZB);
            _RELEASE(pBaseZB);

            _SHOW_REF("refCount:pBaseRT", pBaseRT);
            _RELEASE(pBaseRT);
        }

        const auto& cd = m_ChainDesc;

        UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (m_SupportsVRR) {
            flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        m_pSwapChain->ResizeBuffers(
            cd.BufferCount,
            cd.Width,
            cd.Height,
            cd.Format,
            flags);

        UpdateViews();
        ShowWindow(m_hWnd, SW_MINIMIZE);
    }
}


BOOL CHW::support(D3DFORMAT fmt, DWORD type, DWORD usage)
{
    //	TODO: DX10: implement stub for this code.
    VERIFY(!"Implement CHW::support");
    return TRUE;
}

void CHW::updateWindowProps(HWND m_hWnd)
{
	//	BOOL	bWindowed				= strstr(Core.Params,"-dedicated") ? TRUE : !psDeviceFlags.is	(rsFullscreen);
    BOOL bWindowed = (g_screenmode != 2);

    u32 dwWindowStyle = 0;
    // Set window properties depending on what mode were in.
	if (bWindowed)
	{
		if (m_move_window)
		{
            dwWindowStyle = WS_BORDER | WS_VISIBLE;
            if (!strstr(Core.Params, "-no_dialog_header"))
                dwWindowStyle |= WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX;
            SetWindowLong(m_hWnd, GWL_STYLE, dwWindowStyle);
            // When moving from fullscreen to windowed mode, it is important to
            // adjust the window size after recreating the device rather than
            // beforehand to ensure that you get the window size you want.  For
            // example, when switching from 640x480 fullscreen to windowed with
            // a 1000x600 window on a 1024x768 desktop, it is impossible to set
            // the window size to 1000x600 until after the display mode has
            // changed to 1024x768, because windows cannot be larger than the
            // desktop.

            RECT m_rcWindowBounds;
            RECT DesktopRect;

            GetClientRect(GetDesktopWindow(), &DesktopRect);
            UINT res_width  = m_ChainDesc.Width;
            UINT res_height = m_ChainDesc.Height;

			SetRect(&m_rcWindowBounds,
                (DesktopRect.right - res_width) / 2,
                (DesktopRect.bottom - res_height) / 2,
                (DesktopRect.right + res_width) / 2,
                (DesktopRect.bottom + res_height) / 2);

            AdjustWindowRect(&m_rcWindowBounds, dwWindowStyle, FALSE);

			SetWindowPos(m_hWnd,
                HWND_NOTOPMOST,
                m_rcWindowBounds.left,
                m_rcWindowBounds.top,
                (m_rcWindowBounds.right - m_rcWindowBounds.left),
                (m_rcWindowBounds.bottom - m_rcWindowBounds.top),
                SWP_SHOWWINDOW | SWP_NOCOPYBITS | SWP_DRAWFRAME);
        }
	}
	else
	{
        SetWindowLong(m_hWnd, GWL_STYLE, dwWindowStyle = (WS_POPUP | WS_VISIBLE));
    }

    ShowCursor(FALSE);
    SetForegroundWindow(m_hWnd);
    RECT winRect;
    GetClientRect(m_hWnd, &winRect);
    MapWindowPoints(m_hWnd, nullptr, reinterpret_cast<LPPOINT>(&winRect), 2);
    ClipCursor(&winRect);
}


struct _uniq_mode
    {
	_uniq_mode(LPCSTR v): _val(v)
	{
    }

	LPCSTR _val;
	bool operator()(LPCSTR _other) { return !stricmp(_val, _other); }
};

void free_vid_mode_list()
{
	for (int i = 0; vid_mode_token[i].name; i++)
	{
        xr_free(vid_mode_token[i].name);
    }
    xr_free(vid_mode_token);
    vid_mode_token = NULL;
}

void fill_vid_mode_list(CHW* _hw)
{
	if (vid_mode_token != NULL) return;
	xr_vector<LPCSTR> _tmp;
    xr_vector<DXGI_MODE_DESC> modes;

    IDXGIOutput* pOutput;
    //_hw->m_pSwapChain->GetContainingOutput(&pOutput);
    _hw->m_pAdapter->EnumOutputs(0, &pOutput);
    VERIFY(pOutput);

	UINT num = 0;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	UINT flags = 0;

    // Get the number of display modes available
	pOutput->GetDisplayModeList(format, flags, &num, 0);

    // Get the list of display modes
	modes.resize(num);
	pOutput->GetDisplayModeList(format, flags, &num, &modes.front());

    _RELEASE(pOutput);

	for (u32 i = 0; i < num; ++i)
	{
        DXGI_MODE_DESC& desc = modes[i];
		string32 str;

        if (desc.Width < 800)
            continue;

            xr_sprintf(str, sizeof(str), "%dx%d", desc.Width, desc.Height);

        if (_tmp.end() != std::find_if(_tmp.begin(), _tmp.end(), _uniq_mode(str)))
            continue;

        _tmp.push_back(NULL);
        _tmp.back() = xr_strdup(str);
    }

    u32 _cnt = _tmp.size() + 1;

    vid_mode_token = xr_alloc<xr_token>(_cnt);

	vid_mode_token[_cnt - 1].id = -1;
    vid_mode_token[_cnt - 1].name = NULL;
	for (u32 i = 0; i < _tmp.size(); ++i)
	{
		vid_mode_token[i].id = i;
        vid_mode_token[i].name = _tmp[i];
    }
}

void CHW::UpdateViews()
{
    DXGI_SWAP_CHAIN_DESC1& sd = m_ChainDesc;
	HRESULT R;

    // Create a render target view
	//R_CHK	(pDevice->GetRenderTarget			(0,&pBaseRT));
    ID3DTexture2D* pBuffer;
	R = m_pSwapChain->GetBuffer(0, __uuidof( ID3DTexture2D), (LPVOID*)&pBuffer);
    R_CHK(R);

    R = pDevice->CreateRenderTargetView(pBuffer, NULL, &pBaseRT);
    pBuffer->Release();
    R_CHK(R);

    //	Create Depth/stencil buffer
    //	HACK: DX10: hard depth buffer format
	//R_CHK	(pDevice->GetDepthStencilSurface	(&pBaseZB));
	ID3DTexture2D* pDepthStencil = NULL;

    D3D_TEXTURE2D_DESC descDepth;
	descDepth.Width  = sd.Width;
	descDepth.Height = sd.Height;
	descDepth.MipLevels          = 1;
	descDepth.ArraySize          = 1;
	descDepth.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count   = 1;
    descDepth.SampleDesc.Quality = 0;
	descDepth.Usage              = D3D_USAGE_DEFAULT;
	descDepth.BindFlags          = D3D_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags     = 0;
    descDepth.MiscFlags          = 0;

	R = pDevice->CreateTexture2D(
            &descDepth,      // Texture desc
            NULL,            // Initial data
            &pDepthStencil); // [out] Texture

    R_CHK(R);

    //	Create Depth/stencil view
    R = pDevice->CreateDepthStencilView(pDepthStencil, NULL, &pBaseZB);
    R_CHK(R);

    pDepthStencil->Release();
}