set ver=1.0.2420.47
curl -L -o webview2.nupkg https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/%ver%/microsoft.web.webview2.%ver%.nupkg
mkdir external\nuget\Microsoft.Web.WebView2
tar -xf webview2.nupkg -C external\nuget\Microsoft.Web.WebView2
