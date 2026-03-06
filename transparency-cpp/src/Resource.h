#pragma once

// Icon and cursor IDs
#define IDI_MAIN            100
#define IDC_MAIN_CURSOR     101

// Menu IDs
#define IDM_FILE_SCAN       201
#define IDM_FILE_EXPORT     202
#define IDM_FILE_EXIT       203
#define IDM_VIEW_OVERVIEW   210
#define IDM_VIEW_DEVICES    211
#define IDM_VIEW_ALERTS     212
#define IDM_VIEW_TOOLS      213
#define IDM_VIEW_LEDGER     214
#define IDM_VIEW_PRIVACY    215
#define IDM_HELP_ABOUT      220

// Control IDs
#define IDC_BTN_SCAN_QUICK      1001
#define IDC_BTN_SCAN_DEEP       1002
#define IDC_BTN_SCAN_STANDARD   1003
#define IDC_BTN_MONITOR_START   1004
#define IDC_BTN_MONITOR_STOP    1005
#define IDC_BTN_EXPORT          1006
#define IDC_BTN_SAVE_SNAPSHOT   1007

// Sidebar button IDs
#define IDC_NAV_OVERVIEW    2001
#define IDC_NAV_DEVICES     2002
#define IDC_NAV_ALERTS      2003
#define IDC_NAV_TOOLS       2004
#define IDC_NAV_LEDGER      2005
#define IDC_NAV_PRIVACY     2006

// Tab panel IDs
#define IDC_PANEL_OVERVIEW  3001
#define IDC_PANEL_DEVICES   3002
#define IDC_PANEL_ALERTS    3003
#define IDC_PANEL_TOOLS     3004
#define IDC_PANEL_LEDGER    3005
#define IDC_PANEL_PRIVACY   3006

// ListView IDs
#define IDC_LIST_DEVICES    4001
#define IDC_LIST_ALERTS     4002
#define IDC_LIST_RULES      4003
#define IDC_LIST_LEDGER     4004
#define IDC_LIST_CHANGES    4005

// Edit control IDs
#define IDC_EDIT_DEVICE_NAME    5000
#define IDC_EDIT_SEARCH     5001
#define IDC_EDIT_PING_TARGET    5002
#define IDC_EDIT_PING_COUNT     5003
#define IDC_EDIT_PING_OUTPUT    5004
#define IDC_EDIT_TRACE_TARGET   5005
#define IDC_EDIT_TRACE_OUTPUT   5006
#define IDC_EDIT_DNS_HOST       5007
#define IDC_EDIT_DNS_OUTPUT     5008
#define IDC_EDIT_TCP_HOST       5009
#define IDC_EDIT_TCP_PORT       5010
#define IDC_EDIT_TCP_OUTPUT     5011
#define IDC_EDIT_HTTP_URL       5012
#define IDC_EDIT_HTTP_OUTPUT    5013
#define IDC_EDIT_DEVICE_NOTES   5014
#define IDC_EDIT_STATUS         5015

// Button IDs in tabs
#define IDC_BTN_FILTER_ALL      6001
#define IDC_BTN_FILTER_ONLINE   6002
#define IDC_BTN_FILTER_UNKNOWN  6003
#define IDC_BTN_FILTER_WATCH    6004
#define IDC_BTN_FILTER_OWNED    6005
#define IDC_BTN_FILTER_CHANGED  6006
#define IDC_BTN_PING_RUN        6010
#define IDC_BTN_TRACE_RUN       6011
#define IDC_BTN_DNS_RUN         6012
#define IDC_BTN_TCP_RUN         6013
#define IDC_BTN_HTTP_RUN        6014
#define IDC_BTN_ADD_RULE        6020
#define IDC_BTN_EDIT_RULE       6021
#define IDC_BTN_DEL_RULE        6022
#define IDC_BTN_CLEAR_ALERTS    6023
#define IDC_BTN_BULK_TAG        6030
#define IDC_BTN_BULK_KNOWN      6031
#define IDC_BTN_BULK_WATCH      6032
#define IDC_BTN_BULK_EXPORT     6033
#define IDC_BTN_BULK_FORGET     6034
#define IDC_BTN_DELETE_ALL      6040
#define IDC_BTN_EXPORT_LEDGER   6041
#define IDC_BTN_IMPORT          6042
#define IDC_BTN_DETAIL_PING     6050
#define IDC_BTN_DETAIL_TRACE    6051
#define IDC_BTN_DETAIL_DEEP     6052
#define IDC_BTN_DETAIL_FORGET   6053
#define IDC_BTN_DEVICE_SAVE     6054
#define IDC_BTN_PORT_SCAN_RUN   6060
#define IDC_BTN_WOL_SEND        6061
#define IDC_BTN_REVDNS_RUN      6062
#define IDC_BTN_DIFF_RUN        6063
#define IDC_BTN_API_TOGGLE      6070
#define IDC_BTN_HOOK_ADD        6071
#define IDC_BTN_HOOK_DEL        6072
#define IDC_BTN_SCHED_SAVE      6073

// Combo box IDs
#define IDC_COMBO_DNS_TYPE      7001
#define IDC_COMBO_TRUST         7002
#define IDC_COMBO_SCAN_MODE     7003

// Checkbox IDs
#define IDC_CHECK_GENTLE        8001
#define IDC_CHECK_MONITOR_ENABLE 8002
#define IDC_CHECK_ALERT_OUTAGE  8003
#define IDC_CHECK_ALERT_GATEWAY 8004
#define IDC_CHECK_ALERT_DNS     8005
#define IDC_CHECK_ALERT_LATENCY 8006

// Static text IDs
#define IDC_STATIC_STATUS       9001
#define IDC_STATIC_KPI1         9002
#define IDC_STATIC_KPI2         9003
#define IDC_STATIC_KPI3         9004
#define IDC_STATIC_KPI4         9005
#define IDC_STATIC_NET_INFO     9006
#define IDC_STATIC_VERSION      9007

// Dialog IDs
#define IDD_RULE_BUILDER        10001
#define IDD_ABOUT               10002

// WM_USER custom messages
#define WM_SCAN_COMPLETE        (WM_USER + 1)
#define WM_SCAN_PROGRESS        (WM_USER + 2)
#define WM_MONITOR_TICK         (WM_USER + 3)
#define WM_INTERNET_STATUS      (WM_USER + 4)
#define WM_GATEWAY_CHANGED      (WM_USER + 5)
#define WM_TOOL_RESULT          (WM_USER + 6)
#define WM_ALERT_FIRED          (WM_USER + 7)
#define WM_DNS_CHANGED          (WM_USER + 8)
