#include "debugger.h"


#include <regex>
#include <v8-debug.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;


void Debugger::on_http(websocketpp::connection_hdl hdl) {
    Debugger::DebugServerType::connection_ptr con = this->debug_server.get_con_from_hdl(hdl);

    std::stringstream output;
    output << "<!doctype html><html><body>You requested "
           << con->get_resource()
           << "</body></html>";

    // Set status to 200 rather than the default error code
    con->set_status(websocketpp::http::status_code::ok);
    // Set body text to the HTML created above
    con->set_body(output.str());
}


void Debugger::helper(websocketpp::connection_hdl hdl) {
    std::string response;

//
//
//    this->debug_server.send(hdl, response, websocketpp::frame::opcode::TEXT);
//
//
//    response =
//    this->debug_server.send(hdl, response, websocketpp::frame::opcode::TEXT);

}


void Debugger::on_message(websocketpp::connection_hdl hdl, Debugger::DebugServerType::message_ptr msg) {
    std::smatch matches;
    std::string message = msg->get_payload();
    std::cerr << "Got debugger message: " << message << std::endl;
    nlohmann::json json = json::parse(msg->get_payload());
    v8::Isolate * isolate = this->get_context().get_isolate();
    GLOBAL_CONTEXT_SCOPED_RUN(isolate, this->context->get_global_context());

    if (std::regex_match(message, matches, std::regex("\\s*\\{\"id\":(\\d+),\"method\":\"([^\"]+)\".*"))) {
        int message_id = std::stoi(matches[1]);
        std::string method_name = matches[2];

        std::string response = fmt::format("{{\"id\":{},\"result\":{{}}}}", message_id);

        if (method_name == "Page.getResourceTree") {
            std::cerr << "Message id for resource tree req: " << message_id << std::endl;
            this->debug_server.send(hdl, make_response(message_id, FrameResourceTree(*this)), websocketpp::frame::opcode::TEXT);
            this->debug_server.send(hdl, make_method(Runtime_ExecutionContextCreated(*this)), websocketpp::frame::opcode::TEXT);
            for (auto & script : this->get_context().get_scripts()) {
                this->debug_server.send(hdl, make_method(Debugger_ScriptParsed(*this, *script)),
                                        websocketpp::frame::opcode::TEXT);
            }


        } else if (method_name == "Page.getResourceContent") {
            response = fmt::format("{{\"id\":{},{}}}", message_id, Page_Content("bogus content"));

            response = std::string("{\"id\":") + fmt::format("{}", message_id) +
                       ",\"result\":{\"content\":\"\\u003C!DOCTYPE html PUBLIC \\\"-//W3C//DTD XHTML 1.0 Transitional//EN\\\" \\\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\\\"\\u003E\\n\\u003Chtml xmlns=\\\"http://www.w3.org/1999/xhtml\\\" itemscope=\\\"\\\" itemtype=\\\"http://schema.org/WebPage\\\"\\u003E\\n\\u003Chead\\u003E\\n\\u003Cmeta http-equiv=\\\"X-UA-Compatible\\\" content=\\\"chrome=1\\\" /\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E/* Copyright 2008 Google. */ (function() { (function(){function e(a){this.t={};this.tick=function(a,c,b){this.t[a]=[void 0!=b?b:(new Date).getTime(),c];if(void 0==b)try{window.console.timeStamp(\\\"CSI/\\\"+a)}catch(h){}};this.tick(\\\"start\\\",null,a)}var a,d;window.performance&&(d=(a=window.performance.timing)&&a.responseStart);var f=0\\u003Cd?new e(d):new e;window.jstiming={Timer:e,load:f};if(a){var c=a.navigationStart;0\\u003Cc&&d\\u003E=c&&(window.jstiming.srt=d-c)}if(a){var b=window.jstiming.load;0\\u003Cc&&d\\u003E=c&&(b.tick(\\\"_wtsrt\\\",void 0,c),b.tick(\\\"wtsrt_\\\",\\\"_wtsrt\\\",d),\\nb.tick(\\\"tbsd_\\\",\\\"wtsrt_\\\"))}try{a=null,window.chrome&&window.chrome.csi&&(a=Math.floor(window.chrome.csi().pageT),b&&0\\u003Cc&&(b.tick(\\\"_tbnd\\\",void 0,window.chrome.csi().startE),b.tick(\\\"tbnd_\\\",\\\"_tbnd\\\",c))),null==a&&window.gtbExternal&&(a=window.gtbExternal.pageT()),null==a&&window.external&&(a=window.external.pageT,b&&0\\u003Cc&&(b.tick(\\\"_tbnd\\\",void 0,window.external.startE),b.tick(\\\"tbnd_\\\",\\\"_tbnd\\\",c))),a&&(window.jstiming.pt=a)}catch(g){}})(); })()\\n\\u003C/script\\u003E\\n\\u003Clink rel=\\\"shortcut icon\\\" href=\\\"/_/rsrc/1354323194313/favicon.ico\\\" type=\\\"image/x-icon\\\" /\\u003E\\n\\u003Clink rel=\\\"apple-touch-icon\\\" href=\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/app/images/apple-touch-icon.png\\\" type=\\\"image/png\\\" /\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E/* Copyright 2008 Google. */ (function() { function d(b){return document.getElementById(b)}window.byId=d;function g(b){return b.replace(/^\\\\s+|\\\\s+$/g,\\\"\\\")}window.trim=g;var h=[],k=0;window.JOT_addListener=function(b,a,c){var e=new String(k++);b={eventName:b,handler:a,compId:c,key:e};h.push(b);return e};window.JOT_removeListenerByKey=function(b){for(var a=0;a\\u003Ch.length;a++)if(h[a].key==b){h.splice(a,1);break}};window.JOT_removeAllListenersForName=function(b){for(var a=0;a\\u003Ch.length;a++)h[a].eventName==b&&h.splice(a,1)};\\nwindow.JOT_postEvent=function(b,a,c){var e={eventName:b,eventSrc:a||{},payload:c||{}};if(window.JOT_fullyLoaded)for(a=h.length,c=0;c\\u003Ca&&c\\u003Ch.length;c++){var f=h[c];f&&f.eventName==b&&(e.listenerCompId=f.compId||\\\"\\\",(f=\\\"function\\\"==typeof f.handler?f.handler:window[f.handler])&&f(e))}else window.JOT_delayedEvents.push({eventName:b,eventSrc:a,payload:c})};window.JOT_delayedEvents=[];window.JOT_fullyLoaded=!1;\\nwindow.JOT_formatRelativeToNow=function(b,a){var c=((new Date).getTime()-b)/6E4;if(1440\\u003C=c||0\\u003Ec)return null;var e=0;60\\u003C=c&&(c/=60,e=2);2\\u003C=c&&e++;return a?window.JOT_siteRelTimeStrs[e].replace(\\\"__duration__\\\",Math.floor(c)):window.JOT_userRelTimeStrs[e].replace(\\\"__duration__\\\",Math.floor(c))}; })()\\n\\u003C/script\\u003E\\n\\u003Cscript\\u003E\\n\\n  \\n\\n  var breadcrumbs = [{\\\"path\\\":\\\"/chromium-projects\\\",\\\"deleted\\\":false,\\\"title\\\":\\\"Home\\\",\\\"dir\\\":\\\"ltr\\\"}];\\n  var JOT_clearDotPath = 'https://ssl.gstatic.com/sites/p/2a2c4f/system/app/images/cleardot.gif';\\n\\n  \\n  var JOT_userRelTimeStrs = [\\\"a minute ago\\\",\\\"__duration__ minutes ago\\\",\\\"an hour ago\\\",\\\"__duration__ hours ago\\\"];\\n\\n  \\n  \\n\\n  \\n\\n  var webspace = {\\\"enableAnalytics\\\":true,\\\"pageSharingId\\\":\\\"jotspot_page\\\",\\\"enableUniversalAnalytics\\\":false,\\\"sharingPolicy\\\":\\\"OPENED_WITH_INDICATOR\\\",\\\"siteTitle\\\":\\\"The Chromium Projects\\\",\\\"adsensePublisherId\\\":null,\\\"features\\\":{\\\"validateClientGvizDataSourceUrls\\\":true,\\\"contactStoreMigrationPollForGapi\\\":true,\\\"gapiLoaderUtil\\\":true,\\\"moreMobileStyleImprovements\\\":true,\\\"accumulativeBubblePanelCreation\\\":true,\\\"fileCabinetScreenReaderFix\\\":true,\\\"freezeStartPageUpdates\\\":false,\\\"publicGadgetSearchInStratus\\\":true,\\\"pageDrafts\\\":false,\\\"mobileOrientationFix\\\":true,\\\"plusBadge\\\":false,\\\"pdfEmbedSupport\\\":false,\\\"jsClickFix\\\":true},\\\"isPublic\\\":true,\\\"isConsumer\\\":false,\\\"serverFlags\\\":{\\\"cajaBaseUrl\\\":\\\"//www.gstatic.com/caja\\\",\\\"cajaDebugMode\\\":false},\\\"onepickBaseUrl\\\":\\\"https://docs.google.com\\\",\\\"domainAnalyticsAccountId\\\":\\\"\\\",\\\"plusPageId\\\":\\\"\\\",\\\"signInUrl\\\":\\\"https://accounts.google.com/AccountChooser?continue\\\\u003dhttps://sites.google.com/a/chromium.org/dev/chromium-projects\\\\u0026service\\\\u003djotspot\\\",\\\"analyticsAccountId\\\":\\\"UA-5484340-1\\\",\\\"scottyUrl\\\":\\\"/_/upload\\\",\\\"homePath\\\":\\\"/\\\",\\\"siteNoticeUrlEnabled\\\":null,\\\"plusPageUrl\\\":\\\"\\\",\\\"adsensePromoClickedOrSiteIneligible\\\":true,\\\"csiReportUri\\\":\\\"https://gg.google.com/csi\\\",\\\"sharingId\\\":\\\"jotspot\\\",\\\"termsUrl\\\":\\\"//www.google.com/intl/en/policies/terms/\\\",\\\"gvizVersion\\\":1,\\\"editorResources\\\":{\\\"sitelayout\\\":[\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/app/css/sitelayouteditor.css\\\"],\\\"text\\\":[\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/js/codemirror.js\\\",\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/app/css/codemirror_css.css\\\",\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/js/trog_edit__en.js\\\",\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/app/css/trogedit.css\\\",\\\"/_/rsrc/1473980471000/system/app/css/editor.css\\\",\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/app/css/codeeditor.css\\\",\\\"/_/rsrc/1473980471000/system/app/css/camelot/editor-jfk.css\\\"]},\\\"sharingUrlPrefix\\\":\\\"/_/sharing\\\",\\\"isAdsenseEnabled\\\":true,\\\"domain\\\":\\\"chromium.org\\\",\\\"baseUri\\\":\\\"\\\",\\\"name\\\":\\\"dev\\\",\\\"siteTemplateId\\\":false,\\\"siteNoticeRevision\\\":null,\\\"siteNoticeUrlAddress\\\":null,\\\"siteNoticeMessage\\\":null,\\\"page\\\":{\\\"isRtlLocale\\\":false,\\\"canDeleteWebspace\\\":null,\\\"isPageDraft\\\":null,\\\"parentPath\\\":null,\\\"parentWuid\\\":null,\\\"siteLocale\\\":\\\"en\\\",\\\"timeZone\\\":\\\"America/Los_Angeles\\\",\\\"type\\\":\\\"text\\\",\\\"title\\\":\\\"Home\\\",\\\"locale\\\":\\\"en\\\",\\\"wuid\\\":\\\"wuid:gx:10ae433dadbbab13\\\",\\\"revision\\\":23,\\\"path\\\":\\\"/chromium-projects\\\",\\\"isSiteRtlLocale\\\":false,\\\"pageInheritsPermissions\\\":null,\\\"name\\\":\\\"chromium-projects\\\",\\\"canChangePath\\\":false,\\\"state\\\":\\\"\\\",\\\"properties\\\":{},\\\"bidiEnabled\\\":false,\\\"currentTemplate\\\":{\\\"path\\\":\\\"/system/app/pagetemplates/text\\\",\\\"title\\\":\\\"Web Page\\\"}},\\\"canPublishScriptToAnyone\\\":true,\\\"user\\\":{\\\"keyboardShortcuts\\\":true,\\\"sessionIndex\\\":\\\"\\\",\\\"guest_\\\":true,\\\"displayNameOrEmail\\\":\\\"guest\\\",\\\"userName\\\":\\\"guest\\\",\\\"uid\\\":\\\"\\\",\\\"renderMobile\\\":false,\\\"domain\\\":\\\"\\\",\\\"namespace\\\":\\\"\\\",\\\"hasWriteAccess\\\":false,\\\"namespaceUser\\\":false,\\\"primaryEmail\\\":\\\"guest\\\",\\\"hasAdminAccess\\\":false},\\\"gadgets\\\":{\\\"baseUri\\\":\\\"/system/app/pages/gadgets\\\"}};\\n  webspace.page.breadcrumbs = breadcrumbs;\\n\\n  \\n  var JOT_siteRelTimeStrs = [\\\"a minute ago\\\",\\\"__duration__ minutes ago\\\",\\\"an hour ago\\\",\\\"__duration__ hours ago\\\"];\\n\\n\\u003C/script\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E\\n                window.jstiming.load.tick('scl');\\n              \\u003C/script\\u003E\\n\\u003Cmeta name=\\\"title\\\" content=\\\"The Chromium Projects\\\" /\\u003E\\n\\u003Cmeta itemprop=\\\"name\\\" content=\\\"The Chromium Projects\\\" /\\u003E\\n\\u003Cmeta property=\\\"og:title\\\" content=\\\"The Chromium Projects\\\" /\\u003E\\n\\u003Cmeta name=\\\"description\\\" content=\\\"Home of the Chromium Open Source Project\\\" /\\u003E\\n\\u003Cmeta itemprop=\\\"description\\\" content=\\\"Home of the Chromium Open Source Project\\\" /\\u003E\\n\\u003Cmeta id=\\\"meta-tag-description\\\" property=\\\"og:description\\\" content=\\\"Home of the Chromium Open Source Project\\\" /\\u003E\\n\\u003Cstyle type=\\\"text/css\\\"\\u003E\\n\\u003C/style\\u003E\\n\\u003Clink rel=\\\"stylesheet\\\" type=\\\"text/css\\\" href=\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/app/themes/beigeandblue/standard-css-beigeandblue-ltr-ltr.css\\\" /\\u003E\\n\\u003Clink rel=\\\"stylesheet\\\" type=\\\"text/css\\\" href=\\\"/_/rsrc/1473980471000/system/app/css/overlay.css?cb=beigeandblueundefineda100%25%25150goog-ws-leftthemedefaultstandard\\\" /\\u003E\\n\\u003Clink rel=\\\"stylesheet\\\" type=\\\"text/css\\\" href=\\\"/_/rsrc/1473980471000/system/app/css/camelot/allthemes-view.css\\\" /\\u003E\\n\\u003C!--[if IE]\\u003E\\n          \\u003Clink rel=\\\"stylesheet\\\" type=\\\"text/css\\\" href=\\\"/system/app/css/camelot/allthemes%2die.css\\\" /\\u003E\\n        \\u003C![endif]--\\u003E\\n\\u003Ctitle\\u003EThe Chromium Projects\\u003C/title\\u003E\\n\\u003Cmeta itemprop=\\\"image\\\" content=\\\"https://www.chromium.org/_/rsrc/1438811752264/chromium-projects/logo_chrome_color_1x_web_32dp.png\\\" /\\u003E\\n\\u003Cmeta property=\\\"og:image\\\" content=\\\"https://www.chromium.org/_/rsrc/1438811752264/chromium-projects/logo_chrome_color_1x_web_32dp.png\\\" /\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E\\n                window.jstiming.load.tick('cl');\\n              \\u003C/script\\u003E\\n\\u003C/head\\u003E\\n\\u003Cbody xmlns=\\\"http://www.google.com/ns/jotspot\\\" id=\\\"body\\\" class=\\\" en            \\\"\\u003E\\n\\u003Cdiv id=\\\"sites-page-toolbar\\\" class=\\\"sites-header-divider\\\"\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"sites-status\\\" class=\\\"sites-status\\\" style=\\\"display:none;\\\"\\u003E\\u003Cdiv id=\\\"sites-notice\\\" class=\\\"sites-notice\\\" role=\\\"status\\\" aria-live=\\\"assertive\\\"\\u003E \\u003C/div\\u003E\\u003C/div\\u003E\\n\\u003C/div\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-everything-scrollbar\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-everything\\\" class=\\\"\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-page-wrapper\\\" style=\\\"direction: ltr\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-page-wrapper-inside\\\"\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"sites-chrome-header-wrapper\\\" style=\\\"height:auto;\\\"\\u003E\\n\\u003Ctable id=\\\"sites-chrome-header\\\" class=\\\"sites-layout-hbox\\\" cellspacing=\\\"0\\\" style=\\\"height:auto;\\\"\\u003E\\n\\u003Ctr class=\\\"sites-header-primary-row\\\" id=\\\"sites-chrome-userheader\\\"\\u003E\\n\\u003Ctd id=\\\"sites-header-title\\\" class=\\\"\\\" role=\\\"banner\\\"\\u003E\\u003Cdiv class=\\\"sites-header-cell-buffer-wrapper\\\"\\u003E\\u003Ca href=\\\"https://www.chromium.org/\\\" id=\\\"sites-chrome-userheader-logo\\\"\\u003E\\u003Cimg id=\\\"logo-img-id\\\" src=\\\"/_/rsrc/1438879449147/config/customLogo.gif?revision=3\\\" alt=\\\"The Chromium Projects\\\" class=\\\"sites-logo  \\\" /\\u003E\\u003C/a\\u003E\\u003Ch2\\u003E\\u003Ca href=\\\"https://www.chromium.org/\\\" dir=\\\"ltr\\\" id=\\\"sites-chrome-userheader-title\\\"\\u003EThe Chromium Projects\\u003C/a\\u003E\\u003C/h2\\u003E\\u003C/div\\u003E\\u003C/td\\u003E\\u003Ctd class=\\\"sites-layout-searchbox  \\\"\\u003E\\u003Cdiv class=\\\"sites-header-cell-buffer-wrapper\\\"\\u003E\\u003Cform id=\\\"sites-searchbox-form\\\" action=\\\"/system/app/pages/search\\\" role=\\\"search\\\"\\u003E\\u003Cinput type=\\\"hidden\\\" id=\\\"sites-searchbox-scope\\\" name=\\\"scope\\\" value=\\\"search-site\\\" /\\u003E\\u003Cinput type=\\\"text\\\" id=\\\"jot-ui-searchInput\\\" name=\\\"q\\\" size=\\\"20\\\" value=\\\"\\\" aria-label=\\\"Search this site\\\" /\\u003E\\u003Cdiv id=\\\"sites-searchbox-button-set\\\" class=\\\"goog-inline-block\\\"\\u003E\\u003Cdiv role=\\\"button\\\" id=\\\"sites-searchbox-search-button\\\" class=\\\"goog-inline-block jfk-button jfk-button-standard\\\" tabindex=\\\"0\\\"\\u003ESearch this site\\u003C/div\\u003E\\u003C/div\\u003E\\u003C/form\\u003E\\u003C/div\\u003E\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003Ctr class=\\\"sites-header-secondary-row\\\" id=\\\"sites-chrome-horizontal-nav\\\"\\u003E\\n\\u003Ctd colspan=\\\"2\\\" id=\\\"sites-chrome-header-horizontal-nav-container\\\" role=\\\"navigation\\\"\\u003E\\n\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003C/table\\u003E\\n\\u003C/div\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-main-wrapper\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-main-wrapper-inside\\\"\\u003E\\n\\u003Ctable id=\\\"sites-chrome-main\\\" class=\\\"sites-layout-hbox\\\" cellspacing=\\\"0\\\" cellpadding=\\\"{scmCellpadding}\\\" border=\\\"0\\\"\\u003E\\n\\u003Ctr\\u003E\\n\\u003Ctd id=\\\"sites-chrome-sidebar-left\\\" class=\\\"sites-layout-sidebar-left initial\\\" style=\\\"width:150px\\\"\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"COMP_7648876402527094\\\" class=\\\"sites-embed\\\" role=\\\"navigation\\\"\\u003E\\u003Cdiv class=\\\"sites-embed-content sites-sidebar-nav\\\"\\u003E\\u003Cul role=\\\"navigation\\\" jotId=\\\"navList\\\"\\u003E\\u003Cli class=\\\"nav-first \\\"\\u003E\\u003Cdiv class=\\\"current-bg\\\" jotId=\\\"wuid:gx:10ae433dadbbab13\\\" dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003EHome\\u003C/div\\u003E\\u003C/li\\u003E\\u003Cli class=\\\"\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"/Home\\\" jotId=\\\"wuid:gx:43582b9d2029d3af\\\" class=\\\"sites-navigation-link\\\"\\u003EChromium\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003Cli class=\\\"\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"/chromium-os\\\" jotId=\\\"wuid:gx:83df2ab1f8880ba\\\" class=\\\"sites-navigation-link\\\"\\u003EChromium OS\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003C/ul\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"COMP_14720868319272995\\\" class=\\\"sites-embed\\\" role=\\\"navigation\\\"\\u003E\\u003Ch4 class=\\\"sites-embed-title\\\"\\u003EQuick links\\u003C/h4\\u003E\\u003Cdiv class=\\\"sites-embed-content sites-sidebar-nav\\\"\\u003E\\u003Cul role=\\\"navigation\\\" jotId=\\\"navList\\\"\\u003E\\u003Cli class=\\\"nav-first \\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"http://www.chromium.org/for-testers/bug-reporting-guidelines\\\" class=\\\"sites-navigation-link\\\"\\u003EReport bugs\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003Cli class=\\\"\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"http://www.chromium.org/developers/discussion-groups\\\" class=\\\"sites-navigation-link\\\"\\u003EDiscuss\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003Cli class=\\\"\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"/system/app/pages/sitemap/hierarchy\\\" jotId=\\\"wuid:gx:3534ed7c3a7a3a5b\\\" class=\\\"sites-navigation-link\\\"\\u003ESitemap\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003C/ul\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"COMP_19690813310444355\\\" class=\\\"sites-embed\\\" role=\\\"navigation\\\"\\u003E\\u003Ch4 class=\\\"sites-embed-title\\\"\\u003EOther sites\\u003C/h4\\u003E\\u003Cdiv class=\\\"sites-embed-content sites-sidebar-nav\\\"\\u003E\\u003Cul role=\\\"navigation\\\" jotId=\\\"navList\\\"\\u003E\\u003Cli class=\\\"nav-first \\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"http://blog.chromium.org/\\\" class=\\\"sites-navigation-link\\\"\\u003EChromium Blog\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003Cli class=\\\"\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"http://code.google.com/chrome/extensions/\\\" class=\\\"sites-navigation-link\\\"\\u003EGoogle Chrome Extensions\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003Cli class=\\\"\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\" style=\\\"padding-left: 5px;\\\"\\u003E\\u003Ca href=\\\"https://developers.google.com/chrome/chrome-frame/\\\" class=\\\"sites-navigation-link\\\"\\u003EGoogle Chrome Frame\\u003C/a\\u003E\\u003C/div\\u003E\\u003C/li\\u003E\\u003C/ul\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"COMP_19695218559354544\\\" class=\\\"sites-embed\\\" role=\\\"complementary\\\"\\u003E\\u003Ch4 class=\\\"sites-embed-title\\\"\\u003E\\u003C/h4\\u003E\\u003Cdiv class=\\\"sites-embed-content sites-embed-content-sidebar-textbox\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\"\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003EExcept as otherwise \\u003C/span\\u003E\\u003Ca href=\\\"http://developers.google.com/site-policies.html#restrictions\\\"\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003Enoted\\u003C/span\\u003E\\u003C/a\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003E, the content of this page is licensed under a \\u003C/span\\u003E\\u003Ca href=\\\"http://creativecommons.org/licenses/by/2.5/\\\"\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003ECreative Commons Attribution 2.5 license\\u003C/span\\u003E\\u003C/a\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003E, and examples are licensed under the \\u003C/span\\u003E\\u003Ca href=\\\"http://src.chromium.org/viewvc/chrome/trunk/src/LICENSE\\\" target=\\\"_blank\\\"\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003EBSD License\\u003C/span\\u003E\\u003C/a\\u003E\\u003Cspan style=\\\"font-size:x-small\\\"\\u003E.\\u003Cbr /\\u003E\\u003C/span\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\n\\u003C/td\\u003E\\n\\u003Ctd id=\\\"sites-canvas-wrapper\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-canvas\\\" role=\\\"main\\\"\\u003E\\n\\u003Cdiv id=\\\"goog-ws-editor-toolbar-container\\\"\\u003E \\u003C/div\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"title-crumbs\\\" style=\\\"display: none;\\\"\\u003E\\n\\u003C/div\\u003E\\n\\u003Ch3 xmlns=\\\"http://www.w3.org/1999/xhtml\\\" id=\\\"sites-page-title-header\\\" style=\\\"display: none;\\\" align=\\\"left\\\"\\u003E\\n\\u003Cspan id=\\\"sites-page-title\\\" dir=\\\"ltr\\\" tabindex=\\\"-1\\\" style=\\\"outline: none\\\"\\u003EHome\\u003C/span\\u003E\\n\\u003C/h3\\u003E\\n\\u003Cdiv id=\\\"sites-canvas-main\\\" class=\\\"sites-canvas-main\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-canvas-main-content\\\"\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" class=\\\"sites-layout-name-two-column-hf sites-layout-vbox\\\"\\u003E\\u003Cdiv class=\\\"sites-layout-tile sites-tile-name-header\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\"\\u003EThe Chromium projects include Chromium and Chromium OS, the open-source projects behind the \\u003Ca href=\\\"https://www.google.com/chrome\\\"\\u003EGoogle Chrome\\u003C/a\\u003E browser and Google Chrome OS, respectively. This site houses the documentation and code related to the Chromium projects and is intended for developers interested in learning about and contributing to the open-source projects.\\u003Cbr /\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\u003Ctable cellspacing=\\\"0\\\" class=\\\"sites-layout-hbox\\\"\\u003E\\u003Ctbody\\u003E\\u003Ctr\\u003E\\u003Ctd class=\\\"sites-layout-tile sites-tile-name-content-1\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\"\\u003E\\u003Cb\\u003E\\u003Ca href=\\\"https://www.chromium.org/Home\\\"\\u003E\\u003Cspan style=\\\"font-size:large\\\"\\u003EChromium\\u003C/span\\u003E\\u003C/a\\u003E\\u003Cspan style=\\\"font-size:large\\\"\\u003E \\u003C/span\\u003E\\u003C/b\\u003E\\u003Cbr /\\u003E\\nChromium is an open-source browser project that aims to build a\\nsafer, faster, and more stable way for all users to experience\\nthe web. This site contains design documents, architecture overviews,\\ntesting information, and more to help you learn to build and work with\\nthe Chromium source code.\\n\\u003Cdiv\\u003E\\u003Cbr /\\u003E\\n\\u003C/div\\u003E\\n\\u003Cdiv\\u003E\\n\\u003Ctable border=\\\"0\\\" cellspacing=\\\"0\\\" style=\\\"border-width:0px;border-collapse:collapse\\\"\\u003E\\n\\u003Ctbody\\u003E\\n\\u003Ctr\\u003E\\n\\u003Ctd style=\\\"width:622px;height:55px\\\" valign=\\\"bottom\\\"\\u003E\\n\\u003Ctable border=\\\"0\\\" cellspacing=\\\"0\\\" style=\\\"border-width:0px;border-collapse:collapse\\\"\\u003E\\n\\u003Ctbody\\u003E\\n\\u003Ctr\\u003E\\n\\u003Ctd\\u003E\\u003Ca href=\\\"https://www.google.com/chrome\\\" imageanchor=\\\"1\\\" style=\\\"font-size:13.3333px;background-color:rgb(255,255,255)\\\"\\u003E\\u003Cimg alt=\\\"https://www.google.com/chrome\\\" border=\\\"0\\\" src=\\\"https://www.chromium.org/_/rsrc/1438811752264/chromium-projects/logo_chrome_color_1x_web_32dp.png\\\" /\\u003E\\u003C/a\\u003E\\u003C/td\\u003E\\n\\u003Ctd style=\\\"padding:5px\\\"\\u003E\\u003Cdiv style=\\\"display:block;text-align:left\\\"\\u003E\\u003Cspan style=\\\"font-size:10pt;background-color:transparent\\\"\\u003ELooking for Google Chrome?\\u003C/span\\u003E\\u003C/div\\u003E\\n\\u003Cbr /\\u003E\\n\\u003Ca href=\\\"https://www.google.com/chrome\\\"\\u003EDownload Google Chrome\\u003C/a\\u003E\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003C/tbody\\u003E\\n\\u003C/table\\u003E\\n\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003C/tbody\\u003E\\n\\u003C/table\\u003E\\n\\u003C/div\\u003E\\u003C/div\\u003E\\u003C/td\\u003E\\u003Ctd class=\\\"sites-layout-tile sites-tile-name-content-2\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\"\\u003E\\u003Cb\\u003E\\u003Ca href=\\\"https://www.chromium.org/chromium-os\\\"\\u003E\\u003Cspan style=\\\"font-size:large\\\"\\u003EChromium OS\\u003C/span\\u003E\\u003C/a\\u003E\\u003C/b\\u003E\\n\\u003Cdiv\\u003EChromium OS is an open-source project that aims to provide a fast, simple, and more secure computing experience for people who spend most of their time on the web.\\u00A0 Learn more about the \\u003Ca href=\\\"https://googleblog.blogspot.com/2009/11/releasing-chromium-os-open-source.html\\\"\\u003Eproject goals\\u003C/a\\u003E, obtain the latest build, and learn how you can get involved, submit code, and file bugs.\\u003C/div\\u003E\\n\\u003Cdiv\\u003E\\u003Cbr /\\u003E\\n\\u003C/div\\u003E\\n\\u003Cdiv\\u003E\\n\\u003Ctable border=\\\"0\\\" cellspacing=\\\"0\\\" style=\\\"border-width:0px;border-collapse:collapse\\\"\\u003E\\n\\u003Ctbody\\u003E\\n\\u003Ctr\\u003E\\n\\u003Ctd style=\\\"width:619px;height:55px\\\" valign=\\\"bottom\\\"\\u003E\\n\\u003Ctable border=\\\"0\\\" cellspacing=\\\"0\\\" style=\\\"border-width:0px;border-collapse:collapse\\\" width=\\\"100%\\\"\\u003E\\n\\u003Ctbody\\u003E\\n\\u003Ctr\\u003E\\n\\u003Ctd valign=\\\"bottom\\\" width=\\\"50%\\\"\\u003E\\n\\u003Ctable border=\\\"0\\\" cellspacing=\\\"0\\\" style=\\\"border-width:0px;border-collapse:collapse\\\"\\u003E\\n\\u003Ctbody\\u003E\\n\\u003Ctr\\u003E\\n\\u003Ctd\\u003E\\u003Cdiv style=\\\"display:block;text-align:left\\\"\\u003E\\u003Ca href=\\\"https://www.google.com/chromeos\\\" imageanchor=\\\"1\\\"\\u003E\\u003Cimg alt=\\\"https://www.google.com/chromeos\\\" border=\\\"0\\\" src=\\\"https://www.chromium.org/_/rsrc/1438811752264/chromium-projects/logo_chrome_color_1x_web_32dp.png\\\" /\\u003E\\u003C/a\\u003E\\u003C/div\\u003E\\n\\u003C/td\\u003E\\n\\u003Ctd style=\\\"padding:5px\\\"\\u003ELooking for Google Chrome OS devices?\\u003Cbr /\\u003E\\n\\u003Cbr /\\u003E\\n\\u003Ca href=\\\"https://www.google.com/chromeos\\\"\\u003EVisit the Google Chrome OS site\\u003C/a\\u003E\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003C/tbody\\u003E\\n\\u003C/table\\u003E\\n\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003C/tbody\\u003E\\n\\u003C/table\\u003E\\n\\u003C/td\\u003E\\n\\u003C/tr\\u003E\\n\\u003C/tbody\\u003E\\n\\u003C/table\\u003E\\n\\u003C/div\\u003E\\u003C/div\\u003E\\u003C/td\\u003E\\u003C/tr\\u003E\\u003C/tbody\\u003E\\u003C/table\\u003E\\u003Cdiv class=\\\"sites-layout-tile sites-tile-name-footer sites-layout-empty-tile\\\"\\u003E\\u003Cdiv dir=\\\"ltr\\\"\\u003E\\u003Cbr /\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\u003C/div\\u003E\\n\\u003C/div\\u003E \\n\\u003C/div\\u003E \\n\\u003Cdiv id=\\\"sites-canvas-bottom-panel\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-attachments-container\\\"\\u003E\\n\\u003C/div\\u003E\\n\\u003C/div\\u003E\\n\\u003C/div\\u003E \\n\\u003C/td\\u003E \\n\\u003C/tr\\u003E\\n\\u003C/table\\u003E \\n\\u003C/div\\u003E \\n\\u003C/div\\u003E \\n\\u003Cdiv id=\\\"sites-chrome-footer-wrapper\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-footer-wrapper-inside\\\"\\u003E\\n\\u003Cdiv id=\\\"sites-chrome-footer\\\"\\u003E\\n\\u003C/div\\u003E\\n\\u003C/div\\u003E\\n\\u003C/div\\u003E\\n\\u003C/div\\u003E \\n\\u003C/div\\u003E \\n\\u003Cdiv id=\\\"sites-chrome-adminfooter-container\\\"\\u003E\\n\\u003Cdiv xmlns=\\\"http://www.w3.org/1999/xhtml\\\" class=\\\"sites-adminfooter\\\" role=\\\"navigation\\\"\\u003E\\u003Cp\\u003E\\u003Ca class=\\\"sites-system-link\\\" href=\\\"https://www.google.com/a/UniversalLogin?continue=https://sites.google.com/a/chromium.org/dev/chromium-projects&amp;service=jotspot\\\"\\u003ESign in\\u003C/a\\u003E\\u003Cspan aria-hidden=\\\"true\\\"\\u003E|\\u003C/span\\u003E\\u003Ca class=\\\"sites-system-link\\\" href=\\\"/system/app/pages/recentChanges\\\"\\u003ERecent Site Activity\\u003C/a\\u003E\\u003Cspan aria-hidden=\\\"true\\\"\\u003E|\\u003C/span\\u003E\\u003Ca class=\\\"sites-system-link\\\" href=\\\"/system/app/pages/reportAbuse\\\" target=\\\"_blank\\\"\\u003EReport Abuse\\u003C/a\\u003E\\u003Cspan aria-hidden=\\\"true\\\"\\u003E|\\u003C/span\\u003E\\u003Ca class=\\\"sites-system-link\\\" href=\\\"javascript:;\\\" onclick=\\\"window.open(webspace.printUrl)\\\"\\u003EPrint Page\\u003C/a\\u003E\\u003Cspan aria-hidden=\\\"true\\\"\\u003E|\\u003C/span\\u003E\\u003Cspan class=\\\"sites-system-link\\\"\\u003EPowered By\\u003C/span\\u003E \\u003Cb class=\\\"powered-by\\\"\\u003E\\u003Ca href=\\\"http://sites.google.com\\\"\\u003EGoogle Sites\\u003C/a\\u003E\\u003C/b\\u003E\\u003C/p\\u003E\\u003C/div\\u003E\\n\\u003C/div\\u003E\\n\\u003C/div\\u003E \\n\\u003C/div\\u003E \\n\\u003Cdiv id=\\\"sites-chrome-onebar-footer\\\"\\u003E\\n\\u003C/div\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n    window.jstiming.load.tick('sjl');\\n  \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" src=\\\"https://ssl.gstatic.com/sites/p/2a2c4f/system/js/jot_min_view__en.js\\\"\\u003E\\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n    window.jstiming.load.tick('jl');\\n  \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\"\\u003E\\n      \\n          sites.core.Analytics.createTracker();\\n          sites.core.Analytics.trackPageview();\\n        \\n    \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n                    sites.Searchbox.initialize(\\n                        'sites-searchbox-search-button',\\n                        {\\\"object\\\":[]}['object'],\\n                        'search-site',\\n                        {\\\"label\\\":\\\"Configure search options...\\\",\\\"url\\\":\\\"/system/app/pages/admin/settings\\\"});\\n                  \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n      gsites.HoverPopupMenu.createSiteDropdownMenus('sites-header-nav-dropdown', false);\\n    \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\" defer=\\\"true\\\"\\u003E\\n            JOT_setupNav(\\\"7648876402527094\\\", \\\"Navigation\\\", false);\\n            JOT_addListener('titleChange', 'JOT_NAVIGATION_titleChange', 'COMP_7648876402527094');\\n          \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\" defer=\\\"true\\\"\\u003E\\n            JOT_setupNav(\\\"14720868319272995\\\", \\\"Quick links\\\", false);\\n            JOT_addListener('titleChange', 'JOT_NAVIGATION_titleChange', 'COMP_14720868319272995');\\n          \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\" defer=\\\"true\\\"\\u003E\\n            JOT_setupNav(\\\"19690813310444355\\\", \\\"Other sites\\\", false);\\n            JOT_addListener('titleChange', 'JOT_NAVIGATION_titleChange', 'COMP_19690813310444355');\\n          \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\"\\u003E\\n                    window.onload = function() {\\n                      if (false) {\\n                        JOT_setMobilePreview();\\n                      }\\n                      var loadTimer = window.jstiming.load;\\n                      loadTimer.tick(\\\"ol\\\");\\n                      loadTimer[\\\"name\\\"] = \\\"load,\\\" + webspace.page.type + \\\",user_page\\\";\\n                      window.jstiming.report(loadTimer, {}, 'https://gg.google.com/csi');\\n                    }\\n                  \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n        JOT_insertAnalyticsCode(false,\\n            false);\\n      \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n    var maestroRunner = new gsites.pages.view.SitesMaestroRunner(\\n        webspace, \\\"en\\\");\\n    maestroRunner.initListeners();\\n    maestroRunner.installEditRender();\\n  \\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\" defer=\\\"true\\\"\\u003E\\n  //\\u003C![CDATA[\\n    // Decorate any fastUI buttons on the page with a class of 'goog-button'.\\n    if (webspace.user.hasWriteAccess) {\\n      JOT_decorateButtons();\\n    }\\n\\n    // Fires delayed events.\\n    (function() {\\n      JOT_fullyLoaded = true;\\n      var delayedEvents = JOT_delayedEvents;\\n      for (var x = 0; x \\u003C delayedEvents.length; x++) {\\n        var event = delayedEvents[x];\\n        JOT_postEvent(event.eventName, event.eventSrc, event.payload);\\n      }\\n      JOT_delayedEvents = null;\\n      JOT_postEvent('pageLoaded');\\n    })();\\n  //]]\\u003E\\n\\u003C/script\\u003E\\n\\u003Cscript xmlns=\\\"http://www.w3.org/1999/xhtml\\\" type=\\\"text/javascript\\\"\\u003E\\n    JOT_postEvent('decorateGvizCharts');\\n  \\u003C/script\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E\\n          JOT_setupPostRenderingManager();\\n        \\u003C/script\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E\\n          JOT_postEvent('renderPlus', null, 'sites-chrome-main');\\n        \\u003C/script\\u003E\\n\\u003Cdiv id=\\\"server-timer-div\\\" style=\\\"display:none\\\"\\u003E \\u003C/div\\u003E\\n\\u003Cscript type=\\\"text/javascript\\\"\\u003E\\n          window.jstiming.load.tick('render');\\n          JOT_postEvent('usercontentrendered', this);\\n        \\u003C/script\\u003E\\n\\u003C/body\\u003E\\n\\u003C/html\\u003E\\n\",\"base64Encoded\":false}}";
            this->debug_server.send(hdl, response, websocketpp::frame::opcode::TEXT);

        } else if (method_name == "Debugger.getScriptSource") {
            // the URL for the script comes from Debugger.scriptParsed
            std::cerr << msg->get_payload() << std::endl;
            std::cerr << json << std::endl;
            std::string script_id_string = json["params"]["scriptId"];
            int64_t script_id = std::stol(script_id_string);
            for (auto &script : this->get_context().get_scripts()) {
                if (script->get_script_id() == script_id) {
                    this->debug_server.send(hdl, make_response(message_id, ScriptSource(*script)),
                                            websocketpp::frame::opcode::TEXT);
                }
            }
        } else if (method_name == "Debugger.setBreakpointByUrl") {
            std::string url = json["params"]["url"];
            int line_number = json["params"]["lineNumber"];
            std::cerr << fmt::format("set breakpoint on url: {} line: {}", url, line_number) << std::endl;

            v8toolkit::ScriptPtr script_to_break;
            for (v8toolkit::ScriptPtr const & script : context->get_scripts()) {
                std::cerr << fmt::format("Comparing {} and {}", script->get_source_location(), url) << std::endl;
                if (script->get_source_location() == url) {
                    script_to_break = script;
                    break;
                }
            }
            assert(script_to_break);

            auto debug_context = this->context->get_isolate_helper()->get_debug_context();
            v8::Context::Scope context_scope(*debug_context);
            v8::Local<v8::Value> result =
                    debug_context->run(fmt::format("Debug.setScriptBreakPointById({}, {});", script_to_break->get_script_id(), line_number)).Get(isolate);
            std::cerr << v8toolkit::stringify_value(isolate, result) << std::endl;

            int64_t number = result->ToNumber()->Value();
//            auto result = debug_context->run(fmt::format("Debug.scripts();", url));
           //auto number = v8toolkit::call_simple_javascript_function(isolate, v8toolkit::get_key_as<v8::Function>(*debug_context, result->ToObject(), "number"));
           // auto number = v8toolkit::get_key_as(*debug_context, result->ToObject(), "")
            this->debug_server.send(hdl,
                                    make_response(message_id, Breakpoint(*script_to_break, line_number)),
                                    websocketpp::frame::opcode::TEXT);

        } else if (method_name == "Runtime.evaluate") {
            auto json_params = json["params"];
            int context_id = json_params["contextId"];
            std::string expression = json_params["expression"];
            CONTEXT_SCOPED_RUN(this->get_context().get_context());
            auto result = this->get_context().run(expression);
            this->debug_server.send(hdl, make_response(message_id, RemoteObject(isolate, result.Get(isolate))),
                                    websocketpp::frame::opcode::TEXT);
        } else if (method_name == "Debugger.removeBreakpoint") {
            std::string breakpoint_url = json["params"]["breakpointId"];
            static std::regex breakpoint_id_regex("^(.*):(\\d+):(\\d+)$");
            std::smatch matches;
            if (!std::regex_match(breakpoint_url, matches, breakpoint_id_regex)) {
                assert(false);
            }
            std::string url = matches[1];
            int line_number = std::stoi(matches[2]);
            int column_number = std::stoi(matches[3]);

            // should probably call findBreakPoint
            // scriptBreakPoints() returns all breakpoints
            auto debug_context = this->context->get_isolate_helper()->get_debug_context();
            v8::Context::Scope context_scope(*debug_context);
            v8::Local<v8::Value> result =
                    debug_context->run( fmt::format(R"V0G0N(
                    (
                            function(){{
                                var matching_breakpoints = 0;
                                var comparisons = "comparisons:";
                                var line_number = {};
                                var column_number = {};
                                let scripts = Debug.scriptBreakPoints().forEach(function(current_breakpoint){{
                                    comparisons += ""+current_breakpoint.line() + line_number + current_breakpoint.column() + column_number;
                                    if(current_breakpoint.line() == line_number && (!current_breakpoint.column() || current_breakpoint.column() == column_number)){{
                                        matching_breakpoints++;
                                        Debug.findBreakPoint(current_breakpoint.number(), true); // true disables the breakpoint
                                    }}
                                }});
                                return matching_breakpoints;
                            }})();
            )V0G0N", line_number, column_number)).Get(isolate);
            std::cerr << "breakpoints removed: " <<  v8toolkit::stringify_value(isolate, result) << std::endl;

            // result of scriptBreakPoints():
            // [{active_: true,
            //   break_points_: [
            //     {active_: true,
            //      actual_location: {
            //          column: 4,
            //          line: 4,
            //          script_id: 55},
            //      condition_: null,
            //      script_break_point_: ,
            //      source_position_: 46}
            //    ], // end break_points
            //   column_: undefined,
            //   condition_: undefined,
            //   groupId_: undefined,
            //   line_: 4,
            //   number_: 1,
            //   position_alignment_: 0,
            //   script_id_: 55,
            //   type_: 0
            //  }, {...additional breakpoints...}
            // ]


            this->debug_server.send(hdl, make_response(message_id, ""), websocketpp::frame::opcode::TEXT);


        } else if (method_name == "Debugger.pause") {

        } else if (method_name == "Debugger.setSkipAllPauses") {

        } else if (method_name == "setBreakpointsActive") {
            // active: true/false



        } else {
            this->debug_server.send(hdl, response, websocketpp::frame::opcode::TEXT);
        }



    } else {
        // unknown message format
        assert(false);
    }

}


RemoteObject::RemoteObject(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    this->type = v8toolkit::get_type_string_for_value(value);
    this->value_string = v8toolkit::stringify_value(isolate, value);
//    this->subtype;
//    this->className;
    this->description = *v8::String::Utf8Value(value);
}


Page_Content::Page_Content(std::string const & content) : content(content)
{}

FrameResource::FrameResource(Debugger const & debugger, v8toolkit::Script const & script) : url(script.get_source_location())
{}

Runtime_ExecutionContextDescription::Runtime_ExecutionContextDescription(Debugger const & debugger) :
    frame_id(debugger.get_frame_id()),
    origin(debugger.get_base_url())
{}

FrameResourceTree::FrameResourceTree(Debugger const & debugger) : page_frame(debugger) {
    // nothing to do for child frames at this point, it will always be empty for now

    for (auto & script : debugger.get_context().get_scripts()) {
        this->resources.emplace_back(FrameResource(debugger, *script));
    }
}

PageFrame::PageFrame(Debugger const & debugger) :
        frame_id(debugger.get_frame_id()),
        network_loader_id(debugger.get_frame_id()),
        security_origin(fmt::format("v8toolkit://{}", debugger.get_base_url())),
        url(fmt::format("v8toolkit://{}/", debugger.get_base_url()))
{}

std::string const & Debugger::get_frame_id() const {
    return this->frame_id;
}
std::string Debugger::get_base_url() const {
    return this->context->get_uuid_string();
}

v8toolkit::Context & Debugger::get_context() const {
    return *this->context;
}

Runtime_ExecutionContextCreated::Runtime_ExecutionContextCreated(Debugger const & debugger) :
    execution_context_description(debugger)
{}
Debugger_ScriptParsed::Debugger_ScriptParsed(Debugger const & debugger, v8toolkit::Script const & script) :
    script_id(script.get_script_id()),
    url(script.get_source_location())
{}

ScriptSource::ScriptSource(v8toolkit::Script const & script) :
    source(nlohmann::basic_json<>(script.get_source_code()).dump())
{}

Location::Location(int64_t script_id, int line_number, int column_number) :
    script_id(script_id),
    line_number(line_number),
    column_number(column_number)
{}

Debugger_Paused::Debugger_Paused() {

}


Breakpoint::Breakpoint(v8toolkit::Script const & script, int line_number, int column_number) {
    this->breakpoint_id = fmt::format("{}:{}:{}",script.get_source_location(), line_number, column_number);
    this->locations.emplace_back(Location(script.get_script_id(), line_number, column_number));
}


void debug_event_callback(v8::Debug::EventDetails const & event_details) {
    v8::Isolate * isolate = event_details.GetIsolate();
    // event type ids are here:
    // https://v8docs.nodesource.com/node-0.12/d5/d03/v8-debug_8h_source.html#l00016
    std::cerr << "GOT DEBUG EVENT CALLBACK WITH EVENT TYPE " << event_details.GetEvent() << std::endl;

    v8::Local<v8::Object> event_data = event_details.GetEventData();
    std::cerr << v8toolkit::stringify_value(isolate, event_data) << std::endl;

    v8::Local<v8::Object> execution_state = event_details.GetExecutionState();
    std::cerr << v8toolkit::stringify_value(isolate, execution_state) << std::endl;


}


/**
 * Returning from this function resumes javascript execution
 */
void Debugger::debug_event_callback(v8::Debug::EventDetails const & event_details) {
    v8::Isolate * isolate = event_details.GetIsolate();
    DebugEventCallbackData * callback_data =
            static_cast<DebugEventCallbackData *>(v8::External::Cast(*event_details.GetCallbackData())->Value());
    Debugger & debugger = *callback_data->debugger;

    std::cerr << "GOT DEBUG EVENT CALLBACK WITH EVENT TYPE " << event_details.GetEvent() << std::endl;

    v8::Local<v8::Object> event_data = event_details.GetEventData();
    std::cerr << v8toolkit::stringify_value(isolate, event_data) << std::endl;

    v8::Local<v8::Object> execution_state = event_details.GetExecutionState();
    std::cerr << v8toolkit::stringify_value(isolate, execution_state) << std::endl;

    v8::DebugEvent debug_event_type = event_details.GetEvent();


    if (debug_event_type == v8::DebugEvent::Break) {
        // send message to debugger notifying that breakpoint was hit
        //    callback_data->debugger->send_message(make_method(Debugger_Paused()));

        debugger.paused_on_breakpoint = true;
        int loop_counter = 0;
        while (debugger.paused_on_breakpoint) {
            debugger.debug_server.poll_one();
            usleep(250000);
            if (++loop_counter % 40 == 0) {
                printf("Still waiting for resume from break command from debugger afater %d second\n", loop_counter / 4);
            }
        }
    }
    printf("Resuming from breakpoint\n");

    /* GetEventData() when a breakpoint is hit returns:
     * {break_points_hit_: [{active_: true, actual_location: {column: 4, line: 13, script_id: 55}, condition_: null, script_break_point_: {active_: true, break_points_: [], column_: undefined, condition_: undefined, groupId_: undefined, line_: 13, number_: 1, position_alignment_: 0, script_id_: 55, type_: 0}, source_position_: 175}], frame_: {break_id_: 8, details_: {break_id_: 8, details_: [392424216, {}, function a(){
    println("Beginning of a()");
    let some_var = 5;
    some_var += 5;
    b(some_var);
    println("End of a()");

}, {sourceColumnStart_: [undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, 4, undefined, undefined, undefined, undefined, undefined, undefined, undefined]}, 0, 1, 175, false, false, 0, "some_var", 5]}, index_: 0, type_: "frame"}}

     */

}

void Debugger::on_open(websocketpp::connection_hdl hdl) {
    assert(this->connections.size() == 0);
    this->connections.insert(hdl);
}


void Debugger::on_close(websocketpp::connection_hdl hdl) {
    assert(this->connections.size() == 1);
    this->connections.erase(hdl);
    assert(this->connections.size() == 0);

}


bool Debugger::websocket_validation_handler(websocketpp::connection_hdl hdl) {
    // only allow one connection
    return this->connections.size() == 0;
}

void Debugger::send_message(std::string const & message) {
    if (!this->connections.empty()) {
        this->debug_server.send(*this->connections.begin(), message, websocketpp::frame::opcode::TEXT);
    }
}



Debugger::Debugger(v8toolkit::ContextPtr & context,
                   unsigned short port) :
        context(context),
        port(port)
{

    v8::Isolate * isolate = context->get_isolate();

    GLOBAL_CONTEXT_SCOPED_RUN(isolate, context->get_global_context());
    v8toolkit::add_print(v8::Debug::GetDebugContext(isolate));


    auto data = v8::External::New(isolate, new DebugEventCallbackData(this));
    v8::Debug::SetDebugEventListener(context->get_isolate(), debug_event_callback, data);

    // only allow one connection
    this->debug_server.set_validate_handler(bind(&Debugger::websocket_validation_handler, this,  websocketpp::lib::placeholders::_1));

    // store the connection so events can be sent back to the client without the client sending something first
    this->debug_server.set_open_handler(bind(&Debugger::on_open, this,  websocketpp::lib::placeholders::_1));

    // note that the client disconnected
    this->debug_server.set_close_handler(bind(&Debugger::on_close, this,  websocketpp::lib::placeholders::_1));

    // handle websocket messages from the client
    this->debug_server.set_message_handler(bind(&Debugger::on_message, this, websocketpp::lib::placeholders::_1,websocketpp::lib::placeholders::_2));
    this->debug_server.set_open_handler(websocketpp::lib::bind(&Debugger::on_open, this, websocketpp::lib::placeholders::_1));
    this->debug_server.set_http_handler(websocketpp::lib::bind(&Debugger::on_http, this, websocketpp::lib::placeholders::_1));

    this->debug_server.init_asio();
    this->debug_server.set_reuse_addr(true);
    std::cerr << "Listening on port " << this->port << std::endl;
    this->debug_server.listen(this->port);

    this->debug_server.start_accept();
}

void Debugger::poll() {
    this->debug_server.poll();
}


