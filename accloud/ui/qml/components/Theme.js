.pragma library

// Theme identity
var themeName = "WarmLight"
var accentName = "Teal"

// Mandatory palette tokens (section 2.2)
var bgWindow = "#f7f2e8"
var bgSurface = "#fffaf0"
var bgDialog = "#fffdf7"

var fgPrimary = "#2f2a21"
var fgSecondary = "#6a5f4f"
var fgDisabled = "#9a9184"

var borderDefault = "#d4c7af"
var borderSubtle = "#e5dccb"

var accent = "#0d7f77"
var accentFg = "#f5fffd"

var danger = "#b13b3b"
var warning = "#b27618"
var success = "#2f8a45"

var selectionBg = "#d8ece8"
var selectionFg = "#1f2b29"
var overlayScrim = "#7a000000"
var fgOnDanger = "#fff6f6"
var viewportBg = "#1d1f23"
var viewportBorder = "#3e4351"
var viewportFg = "#d9dde6"

// Typography tokens
var fontTitlePx = 18
var fontSectionPx = 14
var fontBodyPx = 13
var fontCaptionPx = 12

// Layout tokens
var paddingPage = 16
var paddingDialog = 20
var gapRow = 12
var gapSection = 16
var controlHeight = 36
var radiusControl = 8
var radiusDialog = 12
var borderWidth = 1
var tabStrokeWidth = 1
var tabStrokeColor = "#d4c7af"
var tabBaselineColor = "#d4c7af"

// Dialog / overlay helper token
var modalScrimOpacity = 0.35

// Optional helper colors for legacy UI compatibility
var rootGradientStart = "#f7f2e8"
var rootGradientEnd = "#ede3d2"
var panel = "#fffaf0"
var panelStroke = "#d4c7af"
var card = "#fffdf7"
var cardAlt = "#f5ecdc"
var textPrimary = "#2f2a21"
var textSecondary = "#6a5f4f"
var mono = "#3a3226"
var accentStrong = "#0a6a64"
var accentSoft = "#c7ece8"
var warn = "#b27618"
var ok = "#2f8a45"
var thumbStart = "#8dcfc4"
var thumbEnd = "#2f746c"
var shadow = "#22000000"

var _palettes = {
    "WarmLight": {
        bgWindow: "#f7f2e8",
        bgSurface: "#fffaf0",
        bgDialog: "#fffdf7",
        fgPrimary: "#2f2a21",
        fgSecondary: "#6a5f4f",
        fgDisabled: "#9a9184",
        borderDefault: "#d4c7af",
        borderSubtle: "#e5dccb",
        danger: "#b13b3b",
        warning: "#b27618",
        success: "#2f8a45",
        selectionBg: "#d8ece8",
        selectionFg: "#1f2b29",
        rootGradientStart: "#f7f2e8",
        rootGradientEnd: "#ede3d2",
        panel: "#fffaf0",
        card: "#fffdf7",
        cardAlt: "#f5ecdc",
        mono: "#3a3226",
        thumbStart: "#8dcfc4",
        thumbEnd: "#2f746c",
        shadow: "#22000000",
        overlayScrim: "#7a000000",
        fgOnDanger: "#fff6f6",
        viewportBg: "#1d1f23",
        viewportBorder: "#3e4351",
        viewportFg: "#d9dde6",
        modalScrimOpacity: 0.35
    },
    "Dark": {
        bgWindow: "#1f2329",
        bgSurface: "#2a2f36",
        bgDialog: "#323842",
        fgPrimary: "#eef1f4",
        fgSecondary: "#c0c7cf",
        fgDisabled: "#7f8791",
        borderDefault: "#49505b",
        borderSubtle: "#3f4650",
        danger: "#dd6767",
        warning: "#d4a24b",
        success: "#65b97f",
        selectionBg: "#1f5f78",
        selectionFg: "#f2fbff",
        rootGradientStart: "#1f2329",
        rootGradientEnd: "#252b33",
        panel: "#2a2f36",
        card: "#323842",
        cardAlt: "#282d34",
        mono: "#d6dde6",
        thumbStart: "#4c7586",
        thumbEnd: "#2e4b5c",
        shadow: "#66000000",
        overlayScrim: "#7a000000",
        fgOnDanger: "#fff6f6",
        viewportBg: "#171a1f",
        viewportBorder: "#3a404d",
        viewportFg: "#d9dde6",
        modalScrimOpacity: 0.45
    }
}

var _accents = {
    "Teal": {
        accent: "#0d7f77",
        accentFg: "#f5fffd",
        accentStrong: "#0a6a64",
        accentSoft: "#c7ece8"
    },
    "Coral": {
        accent: "#c5644f",
        accentFg: "#fff8f5",
        accentStrong: "#a64f3e",
        accentSoft: "#f2d9d2"
    },
    "Blue": {
        accent: "#2f6ecb",
        accentFg: "#f5f9ff",
        accentStrong: "#275cad",
        accentSoft: "#d6e4fb"
    }
}

function _paletteOrDefault(name) {
    return _palettes[name] !== undefined ? _palettes[name] : _palettes["WarmLight"]
}

function _accentOrDefault(name) {
    return _accents[name] !== undefined ? _accents[name] : _accents["Teal"]
}

function _syncLegacyAliases(palette, accentSet) {
    rootGradientStart = palette.rootGradientStart
    rootGradientEnd = palette.rootGradientEnd
    panel = palette.panel
    panelStroke = palette.borderDefault
    card = palette.card
    cardAlt = palette.cardAlt
    textPrimary = palette.fgPrimary
    textSecondary = palette.fgSecondary
    mono = palette.mono
    accentStrong = accentSet.accentStrong
    accentSoft = accentSet.accentSoft
    warn = palette.warning
    ok = palette.success
    thumbStart = palette.thumbStart
    thumbEnd = palette.thumbEnd
    shadow = palette.shadow
    overlayScrim = palette.overlayScrim
    fgOnDanger = palette.fgOnDanger
    viewportBg = palette.viewportBg
    viewportBorder = palette.viewportBorder
    viewportFg = palette.viewportFg
}

function applyTheme() {
    var palette = _paletteOrDefault(themeName)
    var accentSet = _accentOrDefault(accentName)

    bgWindow = palette.bgWindow
    bgSurface = palette.bgSurface
    bgDialog = palette.bgDialog
    fgPrimary = palette.fgPrimary
    fgSecondary = palette.fgSecondary
    fgDisabled = palette.fgDisabled
    borderDefault = palette.borderDefault
    borderSubtle = palette.borderSubtle
    tabStrokeWidth = borderWidth
    tabStrokeColor = palette.borderDefault
    tabBaselineColor = palette.borderDefault
    danger = palette.danger
    warning = palette.warning
    success = palette.success
    selectionBg = palette.selectionBg
    selectionFg = palette.selectionFg
    modalScrimOpacity = palette.modalScrimOpacity
    overlayScrim = palette.overlayScrim
    fgOnDanger = palette.fgOnDanger
    viewportBg = palette.viewportBg
    viewportBorder = palette.viewportBorder
    viewportFg = palette.viewportFg

    accent = accentSet.accent
    accentFg = accentSet.accentFg

    _syncLegacyAliases(palette, accentSet)
}

function setThemePreset(name) {
    if (_palettes[name] === undefined)
        return false
    themeName = name
    applyTheme()
    return true
}

function setAccent(name) {
    if (_accents[name] === undefined)
        return false
    accentName = name
    applyTheme()
    return true
}

function resetToDefaults() {
    themeName = "WarmLight"
    accentName = "Teal"
    applyTheme()
}

function availableThemePresets() {
    return Object.keys(_palettes)
}

function availableAccents() {
    return Object.keys(_accents)
}

applyTheme()
