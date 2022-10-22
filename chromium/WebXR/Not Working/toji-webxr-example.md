# WebXR Not working in toji-webxr-example

After not successfully showing up WebXR(AR) feature with BabylonJS in chromium, I was thinking maybe I should turn to the simpler, more direct API instead of wrapped API.

We need POC, and https://toji.github.io/webxr-particles/ would be the good example to start with.

Normally, we could test AR by clicking "Enter AR", however, not working in my private build chromium, though it was working in Chrome.

The error message is:

```
> Could not create a session because: The runtime for this configuration could not be installed
> Uncaught (in promise) DOMException: The specified session configuration is not supported.
    Promise.then (async)
    button.onclick @ WebVR.js:133
```

And this is where our journey begins.

## Start with error message

// third_party/blink/renderer/modules/xr/xr_system.cc

After searching the occurrence of the error message in chromium, we locate the place to send the error message.

```
void XRSystem::FinishSessionCreation(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  ...
  if (!result->is_success()) {
    ...
    String error_message =
        String::Format("Could not create a session because: %s",
                       GetConsoleMessage(result->get_failure_reason()));
    AddConsoleMessage(mojom::blink::ConsoleMessageLevel::kError, error_message);   // Error happens here.
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, nullptr);
    return;
  }
  ...
}
```

It seems WebXRSystem try to create session, and when we check the result, we found it didn't success, so we output the error message.

According to the message "Could not create a session because: The runtime for this configuration could not be installed", we locate the result->get_failure_reason() would be:

`device::mojom::RequestSessionError::RUNTIME_INSTALL_FAILURE`

(See  [XRSystem-GetConsoleMessage](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/xr/xr_system.cc;bpv=1;bpt=1;l=371?q=RUNTIME_INSTALL_FAILURE&ss=chromium&gsn=GetConsoleMessage&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%2B%2B%3Fpath%3Dthird_party%2Fblink%2Frenderer%2Fmodules%2Fxr%2Fxr_system.cc%23sFkIB2K-ITg6ER4KiUfhgSnRb8_ixUCb4UB-_ym5xRs))

By logging, we know it has called `XRSystem::RequestImmersiveSession()` with such session_options:

```
mode: kImmersiveAr
required_features:
  REF_SPACE_VIEWER
  REF_SPACE_LOCAL
  REF_SPACE_LOCAL_FLOOR
```

It will call `VRService::RequestSession` at the end, and this function is a mojo call. It leads us to `VRServiceImpl::RequestSession`



## VRServiceImpl::RequestSession

//content/browser/xr/service/vr_service_impl.cc

The failure reason we have known before is `RUNTIME_INSTALL_FAILURE`, and we could easily know where emit such error:

```
void VRServiceImpl::OnInstallResult(SessionRequestData request,
                                    bool install_succeeded) {
  DVLOG(2) << __func__ << ": install_succeeded=" << install_succeeded;

  if (!install_succeeded) {
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::RUNTIME_INSTALL_FAILURE));
    return;
  }
  ...
}
```

Through `CHECK(false)`, we could easily got the call stack:

```
 0173d5f7  content::VRServiceImpl::OnInstallResult(content::VRServiceImpl::SessionRequestData, bool)  ../../content/browser/xr/service/vr_service_impl.cc:644:5
 0173e6ff  void base::internal::FunctorTraits<void (content::VRServiceImpl::*)(content::VRServiceImpl::SessionRequestData, bool), void>::Invoke<void (content::VRServiceImpl::*)(content::VRServiceImpl::SessionRequestData, bool), base::WeakPtr<content::VRServiceImpl>, content::VRServiceImpl::SessionRequestData, bool>(void (content::VRServiceImpl::*)(content::VRServiceImpl::SessionRequestData, bool), base::WeakPtr<content::VRServiceImpl>&&, content::VRServiceImpl::SessionRequestData&&, bool&&)  ../../base/bind_internal.h:608:12
 0173e6c5  void base::internal::InvokeHelper<true, void>::MakeItSo<void (content::VRServiceImpl::*)(content::VRServiceImpl::SessionRequestData, bool), base::WeakPtr<content::VRServiceImpl>, content::VRServiceImpl::SessionRequestData, bool>(void (content::VRServiceImpl::*&&)(content::VRServiceImpl::SessionRequestData, bool), base::WeakPtr<content::VRServiceImpl>&&, content::VRServiceImpl::SessionRequestData&&, bool&&)  ../../base/bind_internal.h:797:5
 0173e67b  base::internal::Invoker<base::internal::BindState<void (content::VRServiceImpl::*)(content::VRServiceImpl::SessionRequestData, bool), base::WeakPtr<content::VRServiceImpl>, content::VRServiceImpl::SessionRequestData>, void (bool)>::RunOnce(base::internal::BindStateBase*, bool)  ../../base/bind_internal.h:819:12
 00d893b5  base::OnceCallback<void (absl::optional<std::Cr::pair<int, int>> const&)>::Run(absl::optional<std::Cr::pair<int, int>> const&) &&  ../../base/callback.h:145:12
 v------>  void base::internal::Invoker<base::internal::BindState<void (net::HostResolverManager::*)(bool), base::WeakPtr<net::HostResolverManager>>, void (bool)>::RunImpl<void (net::HostResolverManager::*)(bool), std::Cr::tuple<base::WeakPtr<net::HostResolverManager>>, 0u>(void (net::HostResolverManager::*&&)(bool), std::Cr::tuple<base::WeakPtr<net::HostResolverManager>>&&, std::Cr::integer_sequence<unsigned int, 0u>, bool&&)  ../../base/bind_internal.h:850:12
 00d9996d  base::internal::Invoker<base::internal::BindState<void (blink::WebMediaPlayerImpl::*)(bool), base::WeakPtr<blink::WebMediaPlayerImpl>>, void (bool)>::RunOnce(base::internal::BindStateBase*, bool)  ../../base/bind_internal.h:819:12
 00d893b5  base::OnceCallback<void (absl::optional<std::Cr::pair<int, int>> const&)>::Run(absl::optional<std::Cr::pair<int, int>> const&) &&  ../../base/callback.h:145:12
 019a64d5  webxr::ArCoreInstallHelper::ShowMessage(int, int)                                 ../../components/webxr/android/arcore_install_helper.cc:91:7
 0173bcd7  content::BrowserXRRuntimeImpl::EnsureInstalled(int, int, base::OnceCallback<void (bool)>)  ../../content/browser/xr/service/browser_xr_runtime_impl.cc:407:20
 0173d571  content::VRServiceImpl::EnsureRuntimeInstalled(content::VRServiceImpl::SessionRequestData, content::BrowserXRRuntimeImpl*)  ../../content/browser/xr/service/vr_service_impl.cc:632:12
 0173d4f7  content::VRServiceImpl::OnPermissionResults(content::VRServiceImpl::SessionRequestData, std::Cr::vector<blink::PermissionType, std::Cr::allocator<blink::PermissionType>> const&, std::Cr::vector<blink::mojom::PermissionStatus, std::Cr::allocator<blink::mojom::PermissionStatus>> const&)  ../../content/browser/xr/service/vr_service_impl.cc:609:3
```

Through the stack trace we could locate the error comes from ArCoreInstallHelper:

//content/browser/xr/service/vr_service_impl.cc

```
void ArCoreInstallHelper::ShowMessage(int render_process_id,
                                      int render_frame_id) {
  DVLOG(1) << __func__;

  ArCoreAvailability availability = static_cast<ArCoreAvailability>(
      Java_ArCoreInstallUtils_getArCoreInstallStatus(AttachCurrentThread()));
  ...
  switch (availability) {
    case ArCoreAvailability::kUnsupportedDeviceNotCapable: {
      RunInstallFinishedCallback(false);            //  <=== Call at here.
      return;  // No need to process further
    }
  ...
}
```

We could easily know it call the Java function ArCoreInstallerUtils.java  `getArCoreInstallStatus()`, and the return value is `kUnsupportedDeviceNotCapable`.

See what happens in Java:

//components/webxr/android/java/src/org/chromium/components/webxr/ArCoreInstallUtils.java

```
    @CalledByNative
    private static @ArCoreAvailability int getArCoreInstallStatus() {
        try {
            return getArCoreShimInstance().checkAvailability(ContextUtils.getApplicationContext());
        } catch (RuntimeException e) {
            Log.w(TAG, "ARCore availability check failed with error: %s", e.toString());
            return ArCoreAvailability.UNSUPPORTED_DEVICE_NOT_CAPABLE;
        }
    }
```

There should be an exception, and let's get it from log:

```
ARCore availability check failed with error: java.lang.RuntimeException: java.lang.ClassNotFoundException: org.chromium.components.webxr.ArCoreShimImpl
```

Oh, interesting, class not found, emmm....

After following the BUILD.gn which includes `ArCoreShimImpl`, and we found it might be related to this variable `_enable_chrome_module`, and monochrome or trichrome could include it.

So magic happens, once I changed the target, it works just fine!

```
gn gen out/release-arm/ && autoninja -C out/release-arm/ monochrome_public_bundle && out/release-arm/bin/monochrome_public_bundle run
```

Good Job !

