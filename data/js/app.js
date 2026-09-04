const I18N = {};

let availableLanguages = [];
let currentLanguage = "en";

const DEFAULT_DEVICE = {
    initialDelaySec: 30,
    packetCount: 5,
    packetIntervalSec: 30,
    udpPort: 9,
    stopWhenReachable: true,
    wakeOnBoot: true,
    enabled: true,
    maxReachabilityChecks: 20,
    pingTimeoutMs: 500,
    broadcast: "",
    secureOn: "",
    category: "",
    notes: ""
};

let editingDeviceId = null;
let logLines = [];
let logInfo = null;
let logAutoRefreshTimer = null;
let logFollowCurrent = true;
let logRefreshInFlight = false;
let networkScanInProgress = false;
const LOG_AUTO_REFRESH_MS = 2000;



let authenticationRequired = false;


function showLoginView() {
    document.getElementById(
        "appShell"
    ).classList.add("hidden");

    document.getElementById(
        "loginView"
    ).classList.remove("hidden");

    document.getElementById(
        "loginPassword"
    ).focus();
}


function showApplication() {
    document.getElementById(
        "loginView"
    ).classList.add("hidden");

    document.getElementById(
        "appShell"
    ).classList.remove("hidden");
}


async function getAuthenticationStatus() {
    const response =
        await fetch(
            "/api/auth/status",
            {cache: "no-store"}
        );

    if (!response.ok) {
        throw new Error(
            "Unable to read authentication status"
        );
    }

    return response.json();
}


async function loginAdministrator(
    password
) {
    const response =
        await fetch(
            "/api/auth/login",
            {
                method: "POST",
                headers: {
                    "Content-Type":
                        "application/json"
                },
                body: JSON.stringify({
                    password: password
                })
            }
        );

    if (!response.ok) {
        let message =
            t("auth.invalidPassword");

        try {
            const body =
                await response.json();

            if (body.message) {
                message =
                    body.message;
            }
        }
        catch (_) {
        }

        throw new Error(message);
    }
}


async function logoutAdministrator() {
    try {
        await fetch(
            "/api/auth/logout",
            {
                method: "POST"
            }
        );
    }
    finally {
        showLoginView();
    }
}


function configureAuthenticationUi() {
    document.getElementById(
        "loginForm"
    ).addEventListener(
        "submit",
        async function (event) {
            event.preventDefault();

            const password =
                document.getElementById(
                    "loginPassword"
                ).value;

            const errorElement =
                document.getElementById(
                    "loginError"
                );

            errorElement.classList.add(
                "hidden"
            );

            try {
                await loginAdministrator(
                    password
                );

                document.getElementById(
                    "loginPassword"
                ).value = "";

                showApplication();

                await initializeAuthenticatedApplication();
            }
            catch (error) {
                errorElement.textContent =
                    error.message;

                errorElement.classList.remove(
                    "hidden"
                );
            }
        }
    );

    document.getElementById(
        "logoutNavLink"
    ).addEventListener(
        "click",
        function (event) {
            event.preventDefault();

            logoutAdministrator();
        }
    );
}


async function initializeAuthenticatedApplication() {
    loadSystemInfo();

    try {
        const setupResponse =
            await fetch(
                "/api/config/wakewizard",
                {cache: "no-store"}
            );

        if (setupResponse.ok) {
            const setupConfig =
                await setupResponse.json();

            if (!setupConfig.provisioned) {
                showView("config");
                return;
            }
        }
        else if (setupResponse.status === 401) {
            showLoginView();
            return;
        }
    }
    catch (error) {
        console.error(error);
    }

    loadDevices();
}



const nativeFetch =
    window.fetch.bind(window);

window.fetch =
    async function (...args) {
        const response =
            await nativeFetch(...args);

        const url =
            typeof args[0] === "string"
                ? args[0]
                : (
                    args[0] &&
                    args[0].url
                        ? args[0].url
                        : ""
                );

        if (
            response.status === 401 &&
            !url.includes(
                "/api/auth/login"
            ) &&
            !url.includes(
                "/api/auth/status"
            )
        ) {
            showLoginView();
        }

        return response;
    };


function parseProperties(text) {
    const result = {};

    text.split(/\r?\n/).forEach(function (line) {
        const trimmed = line.trim();

        if (
            trimmed === "" ||
            trimmed.startsWith("#") ||
            trimmed.startsWith(";")
        ) {
            return;
        }

        const separator = trimmed.indexOf("=");

        if (separator < 0) {
            return;
        }

        const key =
            trimmed.substring(0, separator).trim();

        const value =
            trimmed.substring(separator + 1).trim();

        result[key] = value;
    });

    return result;
}


function t(key) {
    return I18N[key] || key;
}


function applyTranslationsToDocument() {
    document.documentElement.lang =
        currentLanguage;

    document.querySelectorAll(
        "[data-i18n]"
    )
        .forEach(function (element) {
            element.textContent =
                t(element.dataset.i18n);
        });

    document.querySelectorAll(
        "[data-i18n-aria-label]"
    )
        .forEach(function (element) {
            element.setAttribute(
                "aria-label",
                t(
                    element.dataset
                        .i18nAriaLabel
                )
            );
        });

    document.querySelectorAll(
        "[data-i18n-placeholder]"
    )
        .forEach(function (element) {
            element.setAttribute(
                "placeholder",
                t(
                    element.dataset
                        .i18nPlaceholder
                )
            );
        });
}


async function loadLanguageCatalog() {
    const response =
        await fetch(
            "/api/languages",
            {cache: "no-store"}
        );

    if (!response.ok) {
        throw new Error(
            "Unable to load language list"
        );
    }

    const body =
        await response.json();

    availableLanguages =
        Array.isArray(body.languages)
            ? body.languages
            : [];

    currentLanguage =
        String(
            body.current ||
            "en"
        );

    if (
        !availableLanguages.some(
            function (item) {
                return (
                    item.code ===
                    currentLanguage
                );
            }
        )
    ) {
        currentLanguage = "en";
    }
}


function populateLanguageSelector() {
    const select =
        document.getElementById(
            "languageSelect"
        );

    if (!select) {
        return;
    }

    select.innerHTML = "";

    availableLanguages
        .slice()
        .sort(
            function (a, b) {
                return String(a.name)
                    .localeCompare(
                        String(b.name)
                    );
            }
        )
        .forEach(
            function (language) {
                const option =
                    document.createElement(
                        "option"
                    );

                option.value =
                    language.code;

                option.textContent =
                    language.name ||
                    language.code;

                select.appendChild(
                    option
                );
            }
        );

    select.value =
        currentLanguage;
}


async function loadTranslations(
    languageCode = currentLanguage
) {
    let code =
        String(
            languageCode ||
            "en"
        );

    let response =
        await fetch(
            "/lang/" +
            encodeURIComponent(code) +
            ".properties?v=patch011",
            {cache: "no-store"}
        );

    /*
     * English is the safe fallback if a configured language file has been
     * removed from LittleFS or cannot be loaded.
     */
    if (
        !response.ok &&
        code !== "en"
    ) {
        code = "en";

        response =
            await fetch(
                "/lang/en.properties?v=patch011",
                {cache: "no-store"}
            );
    }

    if (!response.ok) {
        throw new Error(
            "Unable to load localization resources: HTTP " +
            response.status
        );
    }

    const parsed =
        parseProperties(
            await response.text()
        );

    for (
        const key of
        Object.keys(I18N)
    ) {
        delete I18N[key];
    }

    Object.assign(
        I18N,
        parsed
    );

    currentLanguage =
        code;

    applyTranslationsToDocument();
    populateLanguageSelector();
}


async function refreshLocalizedDynamicContent() {
    loadSystemInfo();

    const devicesView =
        document.getElementById(
            "devicesView"
        );

    if (
        devicesView &&
        !devicesView.classList.contains(
            "hidden"
        )
    ) {
        loadDevices();
    }

    const logView =
        document.getElementById(
            "logView"
        );

    if (
        logView &&
        !logView.classList.contains(
            "hidden"
        )
    ) {
        renderLogMeta();
        renderLog();
    }

    const systemView =
        document.getElementById(
            "systemView"
        );

    if (
        systemView &&
        !systemView.classList.contains(
            "hidden"
        )
    ) {
        loadSystemPage()
            .catch(console.error);
    }

    const configView =
        document.getElementById(
            "configView"
        );

    if (
        configView &&
        !configView.classList.contains(
            "hidden"
        )
    ) {
        loadWakeWizardConfig();
    }
}


async function saveSelectedLanguage(
    language
) {
    const response =
        await fetch(
            "/api/config/language",
            {
                method: "POST",
                headers: {
                    "Content-Type":
                        "application/json"
                },
                body: JSON.stringify({
                    language: language
                })
            }
        );

    if (!response.ok) {
        let message =
            t("language.changeFailed");

        try {
            const body =
                await response.json();

            if (body.message) {
                message =
                    body.message;
            }
        }
        catch (_) {
        }

        throw new Error(
            message
        );
    }
}


function configureLanguageSelector() {
    const select =
        document.getElementById(
            "languageSelect"
        );

    select.addEventListener(
        "change",
        async function () {
            const previous =
                currentLanguage;

            const selected =
                this.value;

            try {
                await saveSelectedLanguage(
                    selected
                );

                await loadTranslations(
                    selected
                );

                await refreshLocalizedDynamicContent();
            }
            catch (error) {
                console.error(error);

                this.value =
                    previous;

                alert(
                    error.message ||
                    t(
                        "language.changeFailed"
                    )
                );
            }
        }
    );
}




function getWifiSignalClass(rssi) {
    const value =
        Number(rssi);

    if (!Number.isFinite(value)) {
        return "wifi-signal-unknown";
    }

    if (value >= -60) {
        return "wifi-signal-strong";
    }

    if (value >= -70) {
        return "wifi-signal-fair";
    }

    if (value >= -80) {
        return "wifi-signal-low";
    }

    return "wifi-signal-very-low";
}


async function loadSystemInfo() {
    try {
        const response =
            await fetch(
                "/api/system",
                {cache: "no-store"}
            );

        if (!response.ok) {
            throw new Error(
                "System API error"
            );
        }

        const system =
            await response.json();

        const hostname =
            String(
                system.hostname ||
                "wakewizard"
            )
                .trim()
                .toLowerCase();

        const deviceUrl =
            "http://" +
            hostname +
            ".local/";

        const headerUrl =
            document.getElementById(
                "headerDeviceUrl"
            );

        headerUrl.textContent =
            deviceUrl;

        headerUrl.href =
            deviceUrl;

        document.getElementById(
            "ipAddress"
        ).textContent =
            system.ip;

        document.getElementById(
            "firmwareVersion"
        ).textContent =
            " " +
            t("app.versionPrefix") +
            system.version;

        const wifiStatus =
            document.getElementById(
                "wifiStatus"
            );

        const wifiSignalDot =
            document.getElementById(
                "wifiSignalDot"
            );

        wifiSignalDot.classList.remove(
            "wifi-signal-unknown",
            "wifi-signal-strong",
            "wifi-signal-fair",
            "wifi-signal-low",
            "wifi-signal-very-low"
        );

        if (system.wifiConnected) {
            wifiStatus.textContent =
                system.ssid +
                " (" +
                system.rssi +
                " dBm)";

            wifiSignalDot.classList.add(
                getWifiSignalClass(
                    system.rssi
                )
            );

            wifiSignalDot.title =
                t(
                    system.rssi >= -60
                        ? "wifi.strong"
                        : system.rssi >= -70
                            ? "wifi.fair"
                            : system.rssi >= -80
                                ? "wifi.low"
                                : "wifi.veryLow"
                );
        }
        else {
            wifiStatus.textContent =
                t(
                    "system.wifiDisconnected"
                );

            wifiSignalDot.classList.add(
                "wifi-signal-very-low"
            );

            wifiSignalDot.title =
                t(
                    "system.wifiDisconnected"
                );
        }
    }
    catch (error) {
        console.error(error);
    }
}


function configureMenu() {
    const button =
        document.getElementById("menuButton");

    const sidebar =
        document.getElementById("sidebar");

    button.addEventListener(
        "click",
        function () {
            sidebar.classList.toggle("open");
        }
    );
}


function buildConfiguredDeviceCard(device) {
    const card =
        document.createElement("div");

    card.className = "device-card";

    card.innerHTML = `
        <div class="device-name">
            ${escapeHtml(device.name || t("device.unknown"))}
        </div>

        <div class="device-ip">
            ${escapeHtml(device.ip || "-")}
        </div>

        <div class="device-mac">
            ${escapeHtml(device.mac || "-")}
        </div>

        <div class="device-actions">
            <button
                class="wake-button"
                data-device-id="${device.id}">
                ${t("button.wake")}
            </button>

            <button
                class="edit-device-button"
                data-device-id="${device.id}">
                ${t("button.edit")}
            </button>

            <button
                class="delete-device-button"
                data-device-id="${device.id}">
                ${t("button.delete")}
            </button>
        </div>
    `;

    card.querySelector(".wake-button")
        .addEventListener(
            "click",
            async function (event) {
                await wakeDevice(
                    device,
                    event.currentTarget
                );
            }
        );

    card.querySelector(".edit-device-button")
        .addEventListener(
            "click",
            function () {
                openDeviceEditor(device);
            }
        );

    card.querySelector(".delete-device-button")
        .addEventListener(
            "click",
            function () {
                deleteDevice(device);
            }
        );

    return card;
}


async function loadDevices() {
    const deviceList =
        document.getElementById("deviceList");

    try {
        const response =
            await fetch("/api/devices");

        if (!response.ok) {
            throw new Error("Devices API error");
        }

        const devices =
            await response.json();

        deviceList.innerHTML = "";

        if (devices.length === 0) {
            deviceList.innerHTML =
                "<p>" +
                t("status.noDevicesConfigured") +
                "</p>";

            return;
        }

        devices.forEach(
            function (device) {
                deviceList.appendChild(
                    buildConfiguredDeviceCard(device)
                );
            }
        );
    }
    catch (error) {
        console.error(error);

        deviceList.innerHTML =
            "<p>" +
            t("status.unableToLoadDevices") +
            "</p>";
    }
}


const NETWORK_SCAN_BATCH_SIZE = 8;
const NETWORK_SCAN_BATCH_PAUSE_MS = 25;
const NETWORK_SCAN_MAX_ADDRESSES = 254;


function ipv4ToNumber(value) {
    if (!isValidIpv4Address(value)) {
        return null;
    }

    return String(value)
        .trim()
        .split(".")
        .reduce(
            function (result, part) {
                return (
                    result * 256 +
                    Number(part)
                ) >>> 0;
            },
            0
        );
}


function numberToIpv4(value) {
    const address = value >>> 0;

    return [
        (address >>> 24) & 0xFF,
        (address >>> 16) & 0xFF,
        (address >>> 8) & 0xFF,
        address & 0xFF
    ].join(".");
}


async function resolveNetworkScanRange(
    requestedStart,
    requestedEnd
) {
    const response =
        await fetch(
            "/api/system",
            {cache: "no-store"}
        );

    if (!response.ok) {
        throw new Error(
            t("status.unableToScanNetwork")
        );
    }

    const system = await response.json();

    if (!system.wifiConnected) {
        throw new Error(
            "Wi-Fi is not connected"
        );
    }

    const local =
        ipv4ToNumber(system.ip);

    const mask =
        ipv4ToNumber(system.subnetMask);

    if (local === null || mask === null) {
        throw new Error(
            t("status.unableToScanNetwork")
        );
    }

    const network =
        (local & mask) >>> 0;

    const broadcast =
        (network | (~mask)) >>> 0;

    const start =
        requestedStart !== ""
            ? ipv4ToNumber(requestedStart)
            : (network + 1) >>> 0;

    const end =
        requestedEnd !== ""
            ? ipv4ToNumber(requestedEnd)
            : (broadcast - 1) >>> 0;

    if (start === null || end === null) {
        throw new Error(
            t("status.unableToScanNetwork")
        );
    }

    if (
        ((start & mask) >>> 0) !== network ||
        ((end & mask) >>> 0) !== network
    ) {
        throw new Error(
            "Scan range must be inside the local subnet"
        );
    }

    if (
        start <= network ||
        start >= broadcast ||
        end <= network ||
        end >= broadcast
    ) {
        throw new Error(
            "Network and broadcast addresses cannot be scanned"
        );
    }

    if (start > end) {
        throw new Error(
            "Start IP must not be greater than End IP"
        );
    }

    const count = end - start + 1;

    if (count > NETWORK_SCAN_MAX_ADDRESSES) {
        throw new Error(
            "Scan range is limited to 254 addresses"
        );
    }

    return {
        start: start,
        end: end
    };
}


async function scanNetwork(
    startIp = "",
    endIp = ""
) {
    networkScanInProgress = true;

    const advanced =
        startIp !== "" ||
        endIp !== "";

    const button =
        document.getElementById(
            advanced
                ? "advancedScanButton"
                : "scanNetworkButton"
        );

    const results =
        document.getElementById(
            "scanResults"
        );

    const list =
        document.getElementById(
            "scanDeviceList"
        );

    button.disabled = true;
    button.textContent =
        t("status.scanningButton");

    results.classList.remove(
        "hidden"
    );

    list.innerHTML = `
        <div class="scan-loading">
            <span>${t("status.scanningNetwork")}</span>

            <span class="loading-dots">
                <span>.</span>
                <span>.</span>
                <span>.</span>
            </span>
        </div>
    `;

    try {
        const range =
            await resolveNetworkScanRange(
                startIp,
                endIp
            );

        const devices = [];
        const seenDevices = new Set();

        for (
            let batchStart = range.start;
            batchStart <= range.end;
            batchStart += NETWORK_SCAN_BATCH_SIZE
        ) {
            const batchEnd =
                Math.min(
                    batchStart +
                    NETWORK_SCAN_BATCH_SIZE - 1,
                    range.end
                );

            const params =
                new URLSearchParams({
                    start:
                        numberToIpv4(
                            batchStart
                        ),
                    end:
                        numberToIpv4(
                            batchEnd
                        )
                });

            const response =
                await fetch(
                    "/api/scan?" +
                    params.toString(),
                    {cache: "no-store"}
                );

            if (!response.ok) {
                let backendMessage =
                    t(
                        "status.unableToScanNetwork"
                    );

                try {
                    const body =
                        await response.json();

                    if (body.message) {
                        backendMessage =
                            body.message;
                    }
                }
                catch (_) {
                }

                throw new Error(
                    backendMessage
                );
            }

            const batchDevices =
                await response.json();

            batchDevices.forEach(
                function (device) {
                    const normalizedMac =
                        String(
                            device.mac || ""
                        )
                            .trim()
                            .toUpperCase();

                    const key =
                        String(device.ip || "") +
                        "|" +
                        normalizedMac;

                    if (!seenDevices.has(key)) {
                        seenDevices.add(key);
                        devices.push(device);
                    }
                }
            );

            if (batchEnd < range.end) {
                await new Promise(
                    function (resolve) {
                        setTimeout(
                            resolve,
                            NETWORK_SCAN_BATCH_PAUSE_MS
                        );
                    }
                );
            }
        }

        list.innerHTML = "";

        if (devices.length === 0) {
            list.innerHTML =
                "<p>" +
                t("status.noDevicesFound") +
                "</p>";

            return;
        }

        /*
         * Duplicate MAC detection.
         * Normalize first so differences in case/spacing cannot hide
         * a duplicate returned by the scanner.
         */
        const macOccurrences =
            new Map();

        devices.forEach(
            function (device) {
                const normalizedMac =
                    String(
                        device.mac || ""
                    )
                        .trim()
                        .toUpperCase();

                if (normalizedMac === "") {
                    return;
                }

                macOccurrences.set(
                    normalizedMac,
                    (
                        macOccurrences.get(
                            normalizedMac
                        ) || 0
                    ) + 1
                );
            }
        );

        console.info(
            "WakeWizard duplicate MAC map:",
            Object.fromEntries(
                macOccurrences
            )
        );

        devices.forEach(
            function (device) {
                const row =
                    document.createElement(
                        "div"
                    );

                const normalizedMac =
                    String(
                        device.mac || ""
                    )
                        .trim()
                        .toUpperCase();

                const duplicateMac =
                    normalizedMac !== "" &&
                    (
                        macOccurrences.get(
                            normalizedMac
                        ) || 0
                    ) > 1;

                row.className =
                    "scan-device" +
                    (
                        duplicateMac
                            ? " duplicate-mac-row"
                            : ""
                    );

                const hasName =
                    device.name &&
                    device.name.trim() !== "";

                const deviceName =
                    hasName
                        ? device.name
                        : t("device.unknown");

                const nameElement =
                    document.createElement("div");

                nameElement.className =
                    "scan-name";

                if (!hasName) {
                    nameElement.classList.add(
                        "unknown-device"
                    );
                }

                nameElement.textContent =
                    deviceName;

                const ipElement =
                    document.createElement("div");

                ipElement.className =
                    "scan-ip";

                ipElement.textContent =
                    device.ip;

                const macElement =
                    document.createElement("div");

                macElement.className =
                    "scan-mac";

                const macValue =
                    document.createElement("span");

                macValue.className =
                    "scan-mac-value";

                macValue.textContent =
                    device.mac || "-";

                macElement.appendChild(
                    macValue
                );

                if (duplicateMac) {
                    const duplicateBadge =
                        document.createElement("span");

                    duplicateBadge.className =
                        "duplicate-mac-badge";

                    duplicateBadge.title =
                        t("scan.duplicateMacTooltip");

                    duplicateBadge.textContent =
                        t("scan.duplicateMac");

                    macElement.appendChild(
                        duplicateBadge
                    );
                }

                const actionsElement =
                    document.createElement("div");

                actionsElement.className =
                    "scan-actions";

                const useButton =
                    document.createElement("button");

                useButton.className =
                    "use-device-button";

                useButton.dataset.ip =
                    device.ip;

                useButton.dataset.mac =
                    device.mac || "-";

                useButton.textContent =
                    t("button.use");

                actionsElement.appendChild(
                    useButton
                );

                row.appendChild(nameElement);
                row.appendChild(ipElement);
                row.appendChild(macElement);
                row.appendChild(actionsElement);

                list.appendChild(
                    row
                );

                useButton.addEventListener(
                    "click",
                    function () {
                        openDeviceEditor({
                            ip: device.ip,
                            mac: device.mac,
                            name:
                                hasName
                                    ? device.name
                                    : ""
                        });
                    }
                );
            }
        );
    }
    catch (error) {
        console.error(error);

        list.innerHTML =
            "<p>" +
            escapeHtml(
                error.message ||
                t(
                    "status.unableToScanNetwork"
                )
            ) +
            "</p>";
    }
    finally {
        button.disabled = false;
        button.textContent =
            advanced
                ? t("scan.scanRange")
                : t("button.scanNetwork");

        networkScanInProgress = false;

        /*
         * Network scan is synchronous in the ESP32 backend, so the web server
         * cannot serve Logs while it is running. Refresh immediately after
         * the scan finishes if the Logs page is currently visible.
         */
        const logView =
            document.getElementById(
                "logView"
            );

        if (
            logView &&
            !logView.classList.contains(
                "hidden"
            )
        ) {
            logFollowCurrent = true;
            loadLogFiles(true);
        }
    }
}


function isValidIpv4Address(value) {
    const parts =
        String(value)
            .trim()
            .split(".");

    if (parts.length !== 4) {
        return false;
    }

    return parts.every(
        function (part) {
            if (!/^\d{1,3}$/.test(part)) {
                return false;
            }

            const number =
                Number(part);

            return (
                number >= 0 &&
                number <= 255
            );
        }
    );
}


function configureNetworkScan() {
    const normalButton =
        document.getElementById(
            "scanNetworkButton"
        );

    const toggleButton =
        document.getElementById(
            "advancedScanToggleButton"
        );

    const panel =
        document.getElementById(
            "advancedScanPanel"
        );

    const startInput =
        document.getElementById(
            "advancedScanStart"
        );

    const endInput =
        document.getElementById(
            "advancedScanEnd"
        );

    const validation =
        document.getElementById(
            "advancedScanValidation"
        );

    normalButton.addEventListener(
        "click",
        function () {
            scanNetwork();
        }
    );

    toggleButton.addEventListener(
        "click",
        function () {
            panel.classList.toggle(
                "hidden"
            );

            if (
                !panel.classList.contains(
                    "hidden"
                )
            ) {
                startInput.focus();
            }
        }
    );

    document.getElementById(
        "advancedScanButton"
    ).addEventListener(
        "click",
        function () {
            const start =
                startInput.value.trim();

            const end =
                endInput.value.trim();

            validation.classList.add(
                "hidden"
            );

            if (
                start === "" &&
                end === ""
            ) {
                validation.textContent =
                    t(
                        "scan.enterAtLeastOne"
                    );

                validation.classList.remove(
                    "hidden"
                );
                return;
            }

            if (
                start !== "" &&
                !isValidIpv4Address(start)
            ) {
                validation.textContent =
                    t(
                        "scan.invalidStartIp"
                    );

                validation.classList.remove(
                    "hidden"
                );
                startInput.focus();
                return;
            }

            if (
                end !== "" &&
                !isValidIpv4Address(end)
            ) {
                validation.textContent =
                    t(
                        "scan.invalidEndIp"
                    );

                validation.classList.remove(
                    "hidden"
                );
                endInput.focus();
                return;
            }

            scanNetwork(
                start,
                end
            );
        }
    );
}


function openDeviceEditor(device = {}) {
    const isEdit =
        Number(device.id) > 0;

    editingDeviceId =
        isEdit
            ? Number(device.id)
            : null;

    document.getElementById("deviceEditorTitle").textContent =
        isEdit
            ? t("deviceEditor.title.edit")
            : t("deviceEditor.title.add");

    document.getElementById("deviceName").value =
        device.name || "";

    document.getElementById("deviceMac").value =
        device.mac || "";

    document.getElementById("deviceIp").value =
        device.ip || "";

    document.getElementById("initialDelay").value =
        device.initialDelayMs !== undefined
            ? Number(device.initialDelayMs) / 1000
            : DEFAULT_DEVICE.initialDelaySec;

    document.getElementById("packetCount").value =
        device.packetCount ??
        DEFAULT_DEVICE.packetCount;

    document.getElementById("packetInterval").value =
        device.packetIntervalMs !== undefined
            ? Number(device.packetIntervalMs) / 1000
            : DEFAULT_DEVICE.packetIntervalSec;

    document.getElementById("udpPort").value =
        device.udpPort ??
        DEFAULT_DEVICE.udpPort;

    document.getElementById("maxReachabilityChecks").value =
        device.maxReachabilityChecks ??
        DEFAULT_DEVICE.maxReachabilityChecks;

    document.getElementById("pingTimeoutMs").value =
        device.pingTimeoutMs ??
        DEFAULT_DEVICE.pingTimeoutMs;

    document.getElementById("broadcast").value =
        device.broadcast ??
        DEFAULT_DEVICE.broadcast;

    document.getElementById("secureOn").value =
        device.secureOn ??
        DEFAULT_DEVICE.secureOn;

    document.getElementById("category").value =
        device.category ??
        DEFAULT_DEVICE.category;

    document.getElementById("notes").value =
        device.notes ??
        DEFAULT_DEVICE.notes;

    document.getElementById("stopWhenReachable").checked =
        device.stopWhenReachable ??
        DEFAULT_DEVICE.stopWhenReachable;

    document.getElementById("wakeOnBoot").checked =
        device.wakeOnBoot ??
        DEFAULT_DEVICE.wakeOnBoot;

    document.getElementById("deviceEnabled").checked =
        device.enabled ??
        DEFAULT_DEVICE.enabled;

    document.getElementById("advancedSettings")
        .classList.add("hidden");

    document.getElementById("advancedToggleIcon")
        .textContent = "▼";

    document.getElementById("deviceEditorOverlay")
        .classList.remove("hidden");

    document.getElementById("deviceName")
        .focus();
}


function closeDeviceEditor() {
    editingDeviceId = null;

    document.getElementById("deviceEditorOverlay")
        .classList.add("hidden");
}


function readDeviceEditorPayload() {
    return {
        name:
            document.getElementById("deviceName")
                .value.trim(),

        mac:
            document.getElementById("deviceMac")
                .value.trim(),

        ip:
            document.getElementById("deviceIp")
                .value.trim(),

        initialDelaySec:
            Number(
                document.getElementById("initialDelay")
                    .value
            ),

        packetCount:
            Number(
                document.getElementById("packetCount")
                    .value
            ),

        packetIntervalSec:
            Number(
                document.getElementById("packetInterval")
                    .value
            ),

        udpPort:
            Number(
                document.getElementById("udpPort")
                    .value
            ),

        maxReachabilityChecks:
            Number(
                document.getElementById("maxReachabilityChecks")
                    .value
            ),

        pingTimeoutMs:
            Number(
                document.getElementById("pingTimeoutMs")
                    .value
            ),

        broadcast:
            document.getElementById("broadcast")
                .value.trim(),

        secureOn:
            document.getElementById("secureOn")
                .value.trim(),

        category:
            document.getElementById("category")
                .value.trim(),

        notes:
            document.getElementById("notes")
                .value.trim(),

        stopWhenReachable:
            document.getElementById("stopWhenReachable")
                .checked,

        wakeOnBoot:
            document.getElementById("wakeOnBoot")
                .checked,

        enabled:
            document.getElementById("deviceEnabled")
                .checked
    };
}


async function saveDeviceFromEditor() {
    const payload =
        readDeviceEditorPayload();

    if (payload.name === "") {
        document.getElementById("deviceName").focus();
        return;
    }

    if (payload.mac === "") {
        document.getElementById("deviceMac").focus();
        return;
    }

    const isEdit =
        editingDeviceId !== null;

    const url =
        isEdit
            ? "/api/devices/" + editingDeviceId
            : "/api/devices";

    const method =
        isEdit
            ? "PUT"
            : "POST";

    const saveButton =
        document.getElementById("deviceEditorSave");

    saveButton.disabled = true;

    try {
        const response =
            await fetch(
                url,
                {
                    method: method,
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify(payload)
                }
            );

        if (!response.ok) {
            let backendMessage = "";

            try {
                const body =
                    await response.json();

                backendMessage =
                    body.message || "";
            }
            catch (_) {
                // Ignore non-JSON error bodies.
            }

            console.error(
                "Device save failed",
                response.status,
                backendMessage
            );

            if (response.status === 409) {
                throw new Error(
                    t("message.duplicateMac")
                );
            }

            throw new Error(
                isEdit
                    ? t("message.updateFailed")
                    : t("message.saveFailed")
            );
        }

        closeDeviceEditor();
        await loadDevices();
    }
    catch (error) {
        console.error(error);
        alert(error.message);
    }
    finally {
        saveButton.disabled = false;
    }
}


async function wakeDevice(device, button) {
    const originalText = button.textContent;

    button.disabled = true;
    button.textContent = t("button.waking");

    try {
        const response =
            await fetch(
                "/api/devices/" +
                device.id +
                "/wake",
                {
                    method: "POST"
                }
            );

        if (!response.ok) {
            try {
                const body =
                    await response.json();

                console.error(
                    "Wake request failed",
                    response.status,
                    body.message || ""
                );
            }
            catch (_) {
                console.error(
                    "Wake request failed",
                    response.status
                );
            }

            throw new Error(
                t("message.wakeFailed")
            );
        }

        button.textContent =
            t("button.wakeQueued");

        setTimeout(
            function () {
                button.textContent =
                    originalText;
                button.disabled = false;
            },
            1200
        );
    }
    catch (error) {
        console.error(error);
        alert(error.message);

        button.textContent =
            originalText;
        button.disabled = false;
    }
}


async function wakeAllDevices() {
    const button =
        document.getElementById(
            "wakeAllButton"
        );

    if (
        !confirm(
            t("message.wakeAllConfirm")
        )
    ) {
        return;
    }

    const originalText =
        button.textContent;

    button.disabled = true;
    button.textContent =
        t("button.wakingAll");

    try {
        const response =
            await fetch(
                "/api/devices/wake-all",
                {
                    method: "POST"
                }
            );

        const body =
            await response.json();

        if (
            response.status !== 202 &&
            response.status !== 207
        ) {
            throw new Error(
                body.message ||
                t("message.wakeAllFailed")
            );
        }

        alert(
            t("message.wakeAllQueued")
                .replace(
                    "{queued}",
                    body.queued
                )
                .replace(
                    "{eligible}",
                    body.eligible
                )
        );
    }
    catch (error) {
        console.error(error);
        alert(error.message);
    }
    finally {
        button.textContent =
            originalText;
        button.disabled = false;
    }
}


async function deleteDevice(device) {
    const message =
        t("message.deleteConfirm")
            .replace("{name}", device.name || t("device.unknown"));

    if (!confirm(message)) {
        return;
    }

    try {
        const response =
            await fetch(
                "/api/devices/" + device.id,
                {
                    method: "DELETE"
                }
            );

        if (!response.ok) {
            throw new Error(
                t("message.deleteFailed")
            );
        }

        await loadDevices();
    }
    catch (error) {
        console.error(error);
        alert(error.message);
    }
}


function configureDeviceEditor() {
    document.getElementById(
        "wakeAllButton"
    ).addEventListener(
        "click",
        wakeAllDevices
    );

    document.getElementById("addDeviceButton")
        .addEventListener(
            "click",
            function () {
                openDeviceEditor();
            }
        );

    document.getElementById("deviceEditorClose")
        .addEventListener(
            "click",
            closeDeviceEditor
        );

    document.getElementById("deviceEditorCancel")
        .addEventListener(
            "click",
            closeDeviceEditor
        );

    document.getElementById("advancedToggle")
        .addEventListener(
            "click",
            function () {
                const advanced =
                    document.getElementById("advancedSettings");

                const icon =
                    document.getElementById("advancedToggleIcon");

                advanced.classList.toggle("hidden");

                icon.textContent =
                    advanced.classList.contains("hidden")
                        ? "▼"
                        : "▲";
            }
        );

    document.getElementById("deviceEditorOverlay")
        .addEventListener(
            "click",
            function (event) {
                if (event.target === this) {
                    closeDeviceEditor();
                }
            }
        );

    document.addEventListener(
        "keydown",
        function (event) {
            if (event.key === "Escape") {
                closeDeviceEditor();
            }
        }
    );

    document.getElementById("deviceEditorForm")
        .addEventListener(
            "submit",
            async function (event) {
                event.preventDefault();
                await saveDeviceFromEditor();
            }
        );
}


function configureTooltips() {
    const tooltip =
        document.getElementById("tooltip");

    function showTooltip(event) {
        const element =
            event.currentTarget;

        tooltip.textContent =
            t(element.dataset.tooltipKey);

        tooltip.classList.remove("hidden");

        const rect =
            element.getBoundingClientRect();

        const margin = 10;
        let left = rect.left;
        let top = rect.bottom + margin;

        tooltip.style.left = left + "px";
        tooltip.style.top = top + "px";

        requestAnimationFrame(
            function () {
                const box =
                    tooltip.getBoundingClientRect();

                if (
                    box.right >
                    window.innerWidth - margin
                ) {
                    left =
                        window.innerWidth -
                        box.width -
                        margin;
                }

                if (
                    box.bottom >
                    window.innerHeight - margin
                ) {
                    top =
                        rect.top -
                        box.height -
                        margin;
                }

                tooltip.style.left =
                    Math.max(margin, left) + "px";

                tooltip.style.top =
                    Math.max(margin, top) + "px";
            }
        );
    }

    function hideTooltip() {
        tooltip.classList.add("hidden");
    }

    document.querySelectorAll("[data-tooltip-key]")
        .forEach(
            function (element) {
                element.addEventListener(
                    "mouseenter",
                    showTooltip
                );

                element.addEventListener(
                    "mouseleave",
                    hideTooltip
                );

                element.addEventListener(
                    "focus",
                    showTooltip
                );

                element.addEventListener(
                    "blur",
                    hideTooltip
                );
            }
        );
}




function updateDeviceAddressPreview() {
    const hostnameInput =
        document.getElementById(
            "configHostname"
        );

    const addressLink =
        document.getElementById(
            "configDeviceAddress"
        );

    if (
        !hostnameInput ||
        !addressLink
    ) {
        return;
    }

    const hostname =
        hostnameInput.value
            .trim()
            .toLowerCase();

    const valid =
        /^[a-z0-9](?:[a-z0-9-]{0,30}[a-z0-9])?$/.test(
            hostname
        );

    if (!valid) {
        addressLink.textContent = "-";
        addressLink.removeAttribute("href");
        addressLink.classList.add(
            "config-address-disabled"
        );
        return;
    }

    const address =
        "http://" +
        hostname +
        ".local/";

    addressLink.textContent =
        address;

    addressLink.href =
        address;

    addressLink.classList.remove(
        "config-address-disabled"
    );
}


async function loadWakeWizardConfig() {
    try {
        const [
            cfgResponse,
            sysResponse
        ] = await Promise.all([
            fetch(
                "/api/config/wakewizard",
                {cache: "no-store"}
            ),
            fetch(
                "/api/system",
                {cache: "no-store"}
            )
        ]);

        if (
            !cfgResponse.ok ||
            !sysResponse.ok
        ) {
            throw new Error(
                "Unable to load WakeWizard configuration"
            );
        }

        const cfg =
            await cfgResponse.json();

        const sys =
            await sysResponse.json();

        const hostname =
            String(
                cfg.hostname || "wakewizard"
            );

        const ssid =
            String(
                (
                    cfg.wifi &&
                    cfg.wifi.ssid
                )
                    ? cfg.wifi.ssid
                    : (
                        cfg.ssid ||
                        sys.ssid ||
                        ""
                    )
            );

        const retention =
            Number(
                cfg.logRetentionDays || 8
            );

        const configuredLanguage =
            String(
                cfg.language ||
                currentLanguage ||
                "en"
            );

        if (
            availableLanguages.some(
                function (item) {
                    return (
                        item.code ===
                        configuredLanguage
                    );
                }
            )
        ) {
            document.getElementById(
                "languageSelect"
            ).value =
                configuredLanguage;
        }

        document.getElementById(
            "configWifiPassword"
        ).type = "password";

        document.getElementById(
            "toggleWifiPasswordButton"
        ).textContent =
            t("config.showWifiPassword");

        document.getElementById(
            "configHostname"
        ).value = hostname;

        document.getElementById(
            "configLogRetention"
        ).value = retention;

        updateDeviceAddressPreview();

        const configTitle =
            document.querySelector(
                "#configView .page-header h1"
            );

        if (configTitle) {
            configTitle.textContent =
                cfg.provisioned
                    ? t("page.config")
                    : t("setup.title");
        }

        document.getElementById(
            "configAdminPassword"
        ).required =
            !cfg.provisioned;

        document.getElementById(
            "configAdminPasswordConfirm"
        ).required =
            !cfg.provisioned;

        document.getElementById(
            "configNetworkInfo"
        ).textContent =
            "IP: " +
            (sys.ip || "-") +
            " · MAC: " +
            (sys.mac || "-") +
            " · RSSI: " +
            (
                sys.rssi !== undefined
                    ? sys.rssi
                    : "-"
            ) +
            " dBm" +
            " · Version: " +
            (sys.version || "-");

        window.wakeWizardSetupSsid =
            String(
                sys.setupSsid || ""
            );

        await scanWifiNetworks(
            ssid
        );
    }
    catch (error) {
        console.error(error);
        alert(t("config.loadFailed"));
    }
}


async function scanWifiNetworks(selectedSsid) {
    const select = document.getElementById("configSsid");
    select.innerHTML = "";

    try {
        const response = await fetch("/api/wifi/networks", {cache:"no-store"});
        const body = await response.json();

        const setupSsid =
            String(
                window.wakeWizardSetupSsid ||
                ""
            );

        const networks =
            (body.networks || [])
                .filter(
                    function (network) {
                        return (
                            !setupSsid ||
                            network.ssid !== setupSsid
                        );
                    }
                )
                .sort(
                    (a,b) => b.rssi - a.rssi
                );

        networks.forEach(function(network) {
            const option = document.createElement("option");
            option.value = network.ssid;
            option.dataset.secure =
                network.secure ? "true" : "false";
            option.textContent =
                network.ssid + " (" + network.rssi + " dBm)" +
                (network.secure ? " 🔒" : "");
            select.appendChild(option);
        });

        if (
            selectedSsid &&
            selectedSsid !== setupSsid &&
            !networks.some(
                n => n.ssid === selectedSsid
            )
        ) {
            const option = document.createElement("option");
            option.value = selectedSsid;
            option.textContent = selectedSsid;
            select.insertBefore(option, select.firstChild);
        }
        select.value = selectedSsid || (select.options[0] ? select.options[0].value : "");
    } catch (error) {
        console.error(error);
    }
}

function exportWakeWizardConfiguration() {
    const anchor =
        document.createElement("a");

    anchor.href =
        "/api/config/wakewizard/export";

    anchor.download =
        "wakewizard_config.json";

    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
}


function resetWakeWizardConfigImportUi() {
    const input =
        document.getElementById(
            "wakewizardConfigFileInput"
        );

    const fileName =
        document.getElementById(
            "wakewizardConfigFileName"
        );

    input.value = "";
    fileName.textContent = "";
    fileName.classList.add("hidden");
}


function applyImportedWakeWizardConfigToForm(
    importedConfig
) {
    const hostname =
        String(
            importedConfig.hostname || ""
        ).trim();

    const importedSsid =
        String(
            importedConfig.wifi &&
            importedConfig.wifi.ssid
                ? importedConfig.wifi.ssid
                : (
                    importedConfig.ssid || ""
                )
        );

    const retention =
        Number(
            importedConfig.logRetentionDays || 8
        );

    const importedLanguage =
        String(
            importedConfig.language || ""
        ).trim();

    if (hostname !== "") {
        document.getElementById(
            "configHostname"
        ).value = hostname;

        updateDeviceAddressPreview();
    }

    if (
        Number.isFinite(retention) &&
        retention >= 1 &&
        retention <= 30
    ) {
        document.getElementById(
            "configLogRetention"
        ).value = retention;
    }

    if (importedSsid !== "") {
        const select =
            document.getElementById(
                "configSsid"
            );

        let option =
            Array.from(select.options)
                .find(
                    function (item) {
                        return item.value === importedSsid;
                    }
                );

        if (!option) {
            option =
                document.createElement("option");

            option.value =
                importedSsid;

            option.textContent =
                importedSsid +
                " (" +
                t("config.backup.importedSsid") +
                ")";

            select.insertBefore(
                option,
                select.firstChild
            );
        }

        select.value =
            importedSsid;
    }

    if (
        importedLanguage !== "" &&
        availableLanguages.some(
            function (item) {
                return (
                    item.code ===
                    importedLanguage
                );
            }
        )
    ) {
        document.getElementById(
            "languageSelect"
        ).value =
            importedLanguage;
    }

    // Secrets are never imported into the form.
    document.getElementById(
        "configWifiPassword"
    ).value = "";

    document.getElementById(
        "configAdminPassword"
    ).value = "";

    document.getElementById(
        "configAdminPasswordConfirm"
    ).value = "";

    window.wakewizardImportedConfig = {
        ssid: importedSsid,
        originalSsid:
            window.wakewizardCurrentConfig
                ? window.wakewizardCurrentConfig.ssid
                : ""
    };
}


async function importWakeWizardConfigurationFile(
    file
) {
    try {
        const text =
            await file.text();

        const importedConfig =
            JSON.parse(text);

        if (
            typeof importedConfig !==
            "object" ||
            importedConfig === null
        ) {
            throw new Error(
                t("config.backup.invalidFile")
            );
        }

        const version =
            Number(
                importedConfig.version || 1
            );

        if (
            !Number.isFinite(version) ||
            version < 1
        ) {
            throw new Error(
                t("config.backup.invalidFile")
            );
        }

        applyImportedWakeWizardConfigToForm(
            importedConfig
        );

        const fileName =
            document.getElementById(
                "wakewizardConfigFileName"
            );

        fileName.textContent =
            file.name;

        fileName.classList.remove(
            "hidden"
        );

        alert(
            t("config.backup.importLoaded")
        );
    }
    catch (error) {
        console.error(error);

        alert(
            error.message ||
            t("config.backup.invalidFile")
        );

        resetWakeWizardConfigImportUi();
    }
}


function configureWakeWizardConfigBackup() {
    const exportButton =
        document.getElementById(
            "exportWakeWizardConfigButton"
        );

    const chooseButton =
        document.getElementById(
            "chooseWakeWizardConfigButton"
        );

    const input =
        document.getElementById(
            "wakewizardConfigFileInput"
        );

    exportButton.addEventListener(
        "click",
        exportWakeWizardConfiguration
    );

    chooseButton.addEventListener(
        "click",
        function () {
            input.click();
        }
    );

    input.addEventListener(
        "change",
        function () {
            const file =
                input.files[0];

            if (!file) {
                resetWakeWizardConfigImportUi();
                return;
            }

            importWakeWizardConfigurationFile(
                file
            );
        }
    );
}


function configureWakeWizardConfig() {
    configureWakeWizardConfigBackup();

    document.getElementById(
        "toggleWifiPasswordButton"
    ).addEventListener(
        "click",
        function () {
            const passwordInput =
                document.getElementById(
                    "configWifiPassword"
                );

            const showing =
                passwordInput.type === "text";

            passwordInput.type =
                showing
                    ? "password"
                    : "text";

            this.textContent =
                showing
                    ? t("config.showWifiPassword")
                    : t("config.hideWifiPassword");
        }
    );

    document.getElementById(
        "configHostname"
    ).addEventListener(
        "input",
        updateDeviceAddressPreview
    );

    document.getElementById("wifiRescanButton").addEventListener("click", function() {
        scanWifiNetworks(document.getElementById("configSsid").value);
    });

    document.getElementById("saveWakeWizardConfigButton").addEventListener("click", async function() {
        const adminPassword =
            document.getElementById(
                "configAdminPassword"
            ).value;

        const adminPasswordConfirm =
            document.getElementById(
                "configAdminPasswordConfirm"
            ).value;

        const mismatch =
            document.getElementById(
                "configPasswordMismatch"
            );

        if (
            adminPassword !==
            adminPasswordConfirm
        ) {
            mismatch.classList.remove(
                "hidden"
            );

            document.getElementById(
                "configAdminPasswordConfirm"
            ).focus();

            return;
        }

        mismatch.classList.add(
            "hidden"
        );

        const hostname =
            document.getElementById(
                "configHostname"
            ).value.trim();

        if (
            !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,30}[A-Za-z0-9])?$/.test(
                hostname
            )
        ) {
            alert(
                t("config.hostnameInvalid")
            );
            return;
        }

        const importedState =
            window.wakewizardImportedConfig || null;

        const currentState =
            window.wakewizardCurrentConfig || null;

        const ssidSelect =
            document.getElementById("configSsid");

        const selectedSsidOption =
            ssidSelect.options[
                ssidSelect.selectedIndex
            ] || null;

        const selectedNetworkIsOpen =
            selectedSsidOption !== null &&
            selectedSsidOption.dataset.secure ===
                "false";

        if (
            importedState &&
            currentState &&
            importedState.ssid &&
            importedState.ssid !== currentState.ssid &&
            !selectedNetworkIsOpen &&
            document.getElementById(
                "configWifiPassword"
            ).value === ""
        ) {
            alert(
                t("config.backup.wifiPasswordRequired")
            );

            document.getElementById(
                "configWifiPassword"
            ).focus();

            return;
        }

        const payload = {
            hostname: hostname,
            ssid: ssidSelect.value,
            wifiPassword: document.getElementById("configWifiPassword").value,
            wifiOpen: selectedNetworkIsOpen,
            logRetentionDays: Number(document.getElementById("configLogRetention").value),
            language: document.getElementById("languageSelect").value || currentLanguage,
            adminPassword: adminPassword,
            adminPasswordConfirm: adminPasswordConfirm
        };

        const response = await fetch("/api/config/wakewizard", {
            method: "POST",
            headers: {"Content-Type":"application/json"},
            body: JSON.stringify(payload)
        });

        if (!response.ok) {
            let message = "Unable to save configuration";
            try { const body = await response.json(); message = body.message || message; } catch (_) {}
            alert(message);
            return;
        }

        document.getElementById("configWifiPassword").value = "";
        document.getElementById("configAdminPassword").value = "";
        document.getElementById("configAdminPasswordConfirm").value = "";
        document.getElementById("configPasswordMismatch").classList.add("hidden");

        window.wakewizardImportedConfig = null;

        alert(t("config.saved"));
    });
}


function formatBytes(value) {
    const bytes = Number(value || 0);
    if (bytes < 1024) return bytes + " B";
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
    return (bytes / (1024 * 1024)).toFixed(2) + " MB";
}

function formatUptime(seconds) {
    let s = Number(seconds || 0);
    const days = Math.floor(s / 86400); s %= 86400;
    const hours = Math.floor(s / 3600); s %= 3600;
    const minutes = Math.floor(s / 60);
    return (days ? days + "d " : "") + hours + "h " + minutes + "m";
}

function renderSystemPairs(elementId, pairs) {
    const element = document.getElementById(elementId);
    element.replaceChildren();

    pairs.forEach(function (pair) {
        const row =
            document.createElement("div");

        row.className =
            "system-kv-row";

        const label =
            document.createElement("span");

        label.textContent =
            pair[0];

        const value =
            document.createElement("strong");

        value.textContent =
            pair[1];

        row.appendChild(label);
        row.appendChild(value);
        element.appendChild(row);
    });
}

async function loadSystemPage() {
    const response = await fetch("/api/system", {cache:"no-store"});
    if (!response.ok) throw new Error("Unable to load system information");
    const s = await response.json();

    const systemDeviceUrl =
        s.hostname
            ? "http://" +
              String(s.hostname)
                  .trim()
                  .toLowerCase() +
              ".local/"
            : "-";

    renderSystemPairs("systemDeviceInfo", [
        [t("system.version"), s.version || "-"],
        [t("system.hostname"), s.hostname || "-"],
        [t("system.url"), systemDeviceUrl],
        [t("system.uptime"), formatUptime(s.uptimeSeconds)]
    ]);
    renderSystemPairs("systemHardwareInfo", [
        [t("system.chipModel"), s.chipModel || "-"],
        [t("system.chipRevision"), s.chipRevision ?? "-"],
        [t("system.cpuFrequency"), (s.cpuMHz ?? "-") + " MHz"],
        [t("system.flashSize"), formatBytes(s.flashBytes)],
        [t("system.freeHeap"), formatBytes(s.freeHeapBytes)],
        [t("system.minimumFreeHeap"), formatBytes(s.minFreeHeapBytes)]
    ]);
    renderSystemPairs("systemNetworkInfo", [
        [t("config.ssid"), s.ssid || "-"],
        [t("system.ip"), s.ip || "-"],
        [t("system.subnetMask"), s.subnetMask || "-"],
        [t("system.gateway"), s.gateway || "-"],
        [t("system.mac"), s.mac || "-"],
        [t("system.rssi"), (s.rssi ?? "-") + " dBm"]
    ]);
    renderSystemPairs("systemStorageInfo", [
        [t("system.total"), formatBytes(s.fsTotalBytes)],
        [t("system.used"), formatBytes(s.fsUsedBytes)],
        [t("system.free"), formatBytes(s.fsFreeBytes)]
    ]);
    let ntpStatus =
        t("system.notSynchronized");

    if (s.timeSynchronized) {
        ntpStatus =
            t("system.synchronized");
    }
    else if (s.timeSyncRequested) {
        ntpStatus =
            t("system.waitingForSynchronization");
    }

    renderSystemPairs("systemTimeInfo", [
        [t("system.currentTime"), s.currentUtcTime || "-"],
        [t("system.timezone"), s.timeZone || "UTC"],
        [t("system.ntpStatus"), ntpStatus],
        [t("system.ntpServer"), s.ntpServers || "-"]
    ]);
    document.getElementById("systemFirmwareVersion").textContent = s.version || "-";
}

async function postSystemAction(url, confirmText) {
    if (confirmText && !confirm(confirmText)) return;
    const response = await fetch(url, {method:"POST"});
    if (!response.ok) throw new Error("System action failed");
}

async function uploadSystemImage(inputId, url) {
    const input = document.getElementById(inputId);
    if (!input.files.length) {
        alert(t("system.selectBin"));
        return;
    }
    if (!confirm(t("system.otaConfirm"))) return;

    const status = document.getElementById("otaStatus");
    status.textContent = t("system.uploading");

    const body = new FormData();
    body.append("update", input.files[0]);

    const response = await fetch(url, {method:"POST", body:body});
    if (!response.ok) {
        status.textContent = t("system.uploadFailed");
        return;
    }
    status.textContent = t("system.uploadComplete");
}

function configureSystemPage() {
    document.getElementById("refreshSystemButton").addEventListener("click", function () {
        loadSystemPage().catch(console.error);
    });
    document.getElementById("rebootSystemButton").addEventListener("click", async function () {
        try {
            await postSystemAction("/api/system/reboot", t("system.rebootConfirm"));
            alert(t("system.rebooting"));
        } catch (e) { alert(e.message); }
    });
    document.getElementById("factoryResetSystemButton").addEventListener("click", async function () {
        try {
            await postSystemAction("/api/system/factory-reset", t("system.factoryResetConfirm"));
            alert(t("system.factoryResetStarted"));
        } catch (e) { alert(e.message); }
    });
    document.getElementById("uploadFirmwareButton").addEventListener("click", function () {
        uploadSystemImage("firmwareFileInput", "/api/system/ota/firmware");
    });
    document.getElementById("uploadFilesystemButton").addEventListener("click", function () {
        uploadSystemImage("filesystemFileInput", "/api/system/ota/filesystem");
    });
}


function showView(viewName) {
    const devicesView =
        document.getElementById("devicesView");

    const logView =
        document.getElementById("logView");

    const systemView =
        document.getElementById("systemView");

    const configView =
        document.getElementById("configView");

    const devicesNav =
        document.getElementById("devicesNavLink");

    const logNav =
        document.getElementById("logNavLink");

    const systemNav =
        document.getElementById("systemNavLink");

    const configNav =
        document.getElementById("configNavLink");

    devicesView.classList.toggle(
        "hidden",
        viewName !== "devices"
    );

    logView.classList.toggle(
        "hidden",
        viewName !== "log"
    );

    systemView.classList.toggle(
        "hidden",
        viewName !== "system"
    );

    configView.classList.toggle(
        "hidden",
        viewName !== "config"
    );

    devicesNav.classList.toggle(
        "active",
        viewName === "devices"
    );

    logNav.classList.toggle(
        "active",
        viewName === "log"
    );

    systemNav.classList.toggle(
        "active",
        viewName === "system"
    );

    configNav.classList.toggle(
        "active",
        viewName === "config"
    );

    if (viewName === "system") {
        loadSystemPage().catch(console.error);
    }

    document.getElementById("sidebar")
        .classList.remove("open");

    if (viewName === "log") {
        logFollowCurrent = true;

        if (networkScanInProgress) {
            const container =
                document.getElementById(
                    "logContent"
                );

            if (logLines.length === 0) {
                container.innerHTML =
                    '<p class="log-empty">' +
                    escapeHtml(
                        t("log.pausedDuringScan")
                    ) +
                    "</p>";
            }
        }
        else {
            loadLogFiles();
        }

        startLogAutoRefresh();
    }
    else {
        stopLogAutoRefresh();
    }

    if (viewName === "config") {
        loadWakeWizardConfig();
    }
}


function configureNavigation() {
    document.getElementById("devicesNavLink")
        .addEventListener(
            "click",
            function (event) {
                event.preventDefault();
                showView("devices");
            }
        );

    document.getElementById("logNavLink")
        .addEventListener(
            "click",
            function (event) {
                event.preventDefault();
                showView("log");
            }
        );

    document.getElementById("configNavLink")
        .addEventListener(
            "click",
            function (event) {
                event.preventDefault();
                showView("config");
            }
        );

    document.getElementById("systemNavLink")
        .addEventListener("click", function (event) {
            event.preventDefault();
            showView("system");
        });
}


function escapeHtml(value) {
    return value
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}


function detectLogLevel(line) {
    const match =
        line.match(/\s(INFO|WARN|ERROR)\s/);

    return match ? match[1] : "INFO";
}


function renderLog() {
    const container =
        document.getElementById("logContent");

    const search =
        document.getElementById("logSearchInput")
            .value
            .trim()
            .toLowerCase();

    const level =
        document.getElementById("logLevelSelect")
            .value;

    const filtered =
        logLines.filter(function (line) {
            if (!line) {
                return false;
            }

            if (
                level !== "ALL" &&
                detectLogLevel(line) !== level
            ) {
                return false;
            }

            if (
                search &&
                !line.toLowerCase().includes(search)
            ) {
                return false;
            }

            return true;
        });

    if (filtered.length === 0) {
        container.innerHTML =
            '<p class="log-empty">' +
            escapeHtml(t("log.noEntries")) +
            "</p>";
        return;
    }

    container.innerHTML =
        filtered.map(function (line) {
            const logLevel =
                detectLogLevel(line);

            return (
                '<div class="log-line log-' +
                logLevel.toLowerCase() +
                '">' +
                escapeHtml(line) +
                "</div>"
            );
        }).join("");

    container.scrollTop =
        container.scrollHeight;
}


function formatBytes(bytes) {
    const value = Number(bytes) || 0;

    if (value < 1024) {
        return value + " B";
    }

    if (value < 1024 * 1024) {
        return (value / 1024).toFixed(1) + " KB";
    }

    return (value / (1024 * 1024)).toFixed(2) + " MB";
}


function getSelectedLogFileInfo() {
    if (!logInfo || !Array.isArray(logInfo.files)) {
        return null;
    }

    const select =
        document.getElementById("logFileSelect");

    return logInfo.files.find(function (file) {
        return file.name === select.value;
    }) || null;
}

function getSelectedLogFileName() {
    const select =
        document.getElementById("logFileSelect");

    return select
        ? select.value
        : "";
}

function renderLogMeta() {
    const meta =
        document.getElementById("logMeta");

    if (!logInfo) {
        meta.textContent = "";
        return;
    }

    const maxKb =
        Math.round(logInfo.maxFileBytes / 1024);

    const selectedFile =
        getSelectedLogFileInfo();

    const selectedFileSize =
        selectedFile
            ? formatBytes(selectedFile.size)
            : "-";

    meta.textContent =
        t("log.activeFile") +
        ": " +
        (logInfo.current || "-") +
        " · " +
        t("log.retention") +
        ": " +
        logInfo.retentionDays +
        " " +
        t("log.days") +
        " · " +
        t("log.fileLimit") +
        ": " +
        maxKb +
        " " +
        t("unit.kilobytesShort") +
        " · " +
        t("log.selectedFileSize") +
        ": " +
        selectedFileSize +
        " · " +
        t("log.timeBasis") +
        ": " +
        t("log.timezoneUtc") +
        " · " +
        t("log.persistedLines") +
        ": " +
        (logInfo.persistedLines || 0) +
        " · " +
        t("log.writeFailures") +
        ": " +
        (logInfo.writeFailures || 0);
}


async function loadLogContent(
    fileName,
    silent = false
) {
    const container =
        document.getElementById("logContent");

    if (!silent) {
        container.innerHTML =
            '<p class="log-empty">' +
            escapeHtml(t("log.loading")) +
            "</p>";
    }

    try {
        const response =
            await fetch(
                "/api/logs/file?name=" +
                encodeURIComponent(fileName),
                { cache: "no-store" }
            );

        if (!response.ok) {
            throw new Error(
                t("log.loadFailed")
            );
        }

        const text =
            await response.text();

        logLines =
            text.split(/\r?\n/);

        renderLog();
    }
    catch (error) {
        console.error(error);

        /*
         * Background refresh failures must never erase the last valid log.
         * This is especially important while NetworkScanner is busy because
         * the synchronous scan temporarily blocks WebServer.handleClient().
         */
        if (!silent) {
            container.innerHTML =
                '<p class="log-empty">' +
                escapeHtml(error.message) +
                "</p>";
        }
    }
}


async function loadLogFiles(
    silent = false
) {
    if (logRefreshInFlight) {
        return;
    }

    if (
        networkScanInProgress &&
        silent
    ) {
        return;
    }

    logRefreshInFlight = true;

    const select =
        document.getElementById(
            "logFileSelect"
        );

    const previousValue =
        select.value;

    try {
        const response =
            await fetch(
                "/api/logs/files",
                {cache: "no-store"}
            );

        if (!response.ok) {
            throw new Error(
                t("log.loadFailed")
            );
        }

        const previousCurrent =
            logInfo
                ? logInfo.current
                : "";

        logInfo =
            await response.json();

        const files =
            (logInfo.files || [])
                .slice()
                .sort(function (a, b) {
                    return b.name.localeCompare(
                        a.name
                    );
                });

        select.innerHTML = "";

        files.forEach(
            function (file) {
                const option =
                    document.createElement(
                        "option"
                    );

                option.value =
                    file.name;

                option.textContent =
                    file.name +
                    (
                        file.current
                            ? " (" +
                              t("log.current") +
                              ")"
                            : ""
                    );

                select.appendChild(
                    option
                );
            }
        );

        if (files.length === 0) {
            logLines = [];
            renderLogMeta();
            renderLog();
            updateLogActionButtons();
            return;
        }

        /*
         * If the user was following the current log, follow it even if
         * NTP caused Logger to switch from the unsynced file to a daily file.
         * If the user explicitly selected history, preserve that selection.
         */
        let preferred = "";

        if (
            logFollowCurrent &&
            logInfo.current
        ) {
            preferred =
                logInfo.current;
        }
        else if (
            files.some(
                function (file) {
                    return (
                        file.name ===
                        previousValue
                    );
                }
            )
        ) {
            preferred =
                previousValue;
        }
        else {
            preferred =
                logInfo.current ||
                files[0].name;
        }

        select.value =
            preferred;

        renderLogMeta();
        updateLogActionButtons();

        await loadLogContent(
            select.value,
            silent
        );
    }
    catch (error) {
        console.error(error);

        if (!silent) {
            /*
             * Keep the previous valid log if we already have one. An initial
             * load may still show a friendly temporary-unavailable message.
             */
            if (logLines.length === 0) {
                document.getElementById(
                    "logContent"
                ).innerHTML =
                    '<p class="log-empty">' +
                    escapeHtml(
                        networkScanInProgress
                            ? t("log.pausedDuringScan")
                            : t("log.temporarilyUnavailable")
                    ) +
                    "</p>";
            }
        }
    }
    finally {
        logRefreshInFlight = false;
    }
}


function stopLogAutoRefresh() {
    if (
        logAutoRefreshTimer !==
        null
    ) {
        clearInterval(
            logAutoRefreshTimer
        );

        logAutoRefreshTimer =
            null;
    }
}


function startLogAutoRefresh() {
    stopLogAutoRefresh();

    const enabled =
        document.getElementById(
            "logAutoRefresh"
        ).checked;

    if (!enabled) {
        return;
    }

    logAutoRefreshTimer =
        setInterval(
            function () {
                const logView =
                    document.getElementById(
                        "logView"
                    );

                if (
                    !logView.classList.contains(
                        "hidden"
                    ) &&
                    !networkScanInProgress &&
                    !logRefreshInFlight
                ) {
                    loadLogFiles(true);
                }
            },
            LOG_AUTO_REFRESH_MS
        );
}




function updateLogActionButtons() {
    const hasFile = getSelectedLogFileName() !== "";

    document.getElementById("downloadLogButton").disabled = !hasFile;
    document.getElementById("deleteLogButton").disabled = !hasFile;

    const historicalCount =
        logInfo && Array.isArray(logInfo.files)
            ? logInfo.files.filter(function (file) {
                return !file.current;
            }).length
            : 0;

    document.getElementById(
        "deleteLogHistoryButton"
    ).disabled = historicalCount === 0;
}


function downloadSelectedLog() {
    const fileName = getSelectedLogFileName();

    if (!fileName) {
        return;
    }

    const anchor = document.createElement("a");

    anchor.href =
        "/api/logs/download?name=" +
        encodeURIComponent(fileName);

    anchor.download = fileName;

    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
}


async function deleteSelectedLog() {
    const fileName = getSelectedLogFileName();

    if (!fileName) {
        return;
    }

    if (!confirm(t("log.delete.confirm") + "\\n\\n" + fileName)) {
        return;
    }

    try {
        const response =
            await fetch(
                "/api/logs/file?name=" +
                encodeURIComponent(fileName),
                {
                    method: "DELETE",
                    cache: "no-store"
                }
            );

        if (!response.ok) {
            let message = t("log.delete.failed");

            try {
                const body = await response.json();

                if (body.message) {
                    message = body.message;
                }
            }
            catch (_) {
            }

            throw new Error(message);
        }

        await loadLogFiles();
    }
    catch (error) {
        console.error(error);
        alert(error.message);
    }
}


async function deleteLogHistory() {
    if (!confirm(t("log.history.delete.confirm"))) {
        return;
    }

    try {
        const response =
            await fetch(
                "/api/logs/history",
                {
                    method: "DELETE",
                    cache: "no-store"
                }
            );

        if (!response.ok) {
            throw new Error(
                t("log.history.delete.failed")
            );
        }

        const body = await response.json();

        alert(
            t("log.history.delete.success") +
            ": " +
            (body.deleted || 0)
        );

        await loadLogFiles();
    }
    catch (error) {
        console.error(error);
        alert(error.message);
    }
}


function configureLogs() {
    document.getElementById("downloadLogButton")
        .addEventListener("click", downloadSelectedLog);

    document.getElementById("deleteLogButton")
        .addEventListener("click", deleteSelectedLog);

    document.getElementById("deleteLogHistoryButton")
        .addEventListener("click", deleteLogHistory);

    document.getElementById(
        "refreshLogButton"
    ).addEventListener(
        "click",
        function () {
            if (networkScanInProgress) {
                alert(
                    t("log.pausedDuringScan")
                );
                return;
            }

            loadLogFiles(false);
        }
    );

    document.getElementById(
        "logAutoRefresh"
    ).addEventListener(
        "change",
        function () {
            if (this.checked) {
                startLogAutoRefresh();
                loadLogFiles(true);
            }
            else {
                stopLogAutoRefresh();
            }
        }
    );

    document.getElementById("logFileSelect")
        .addEventListener(
            "change",
            function (event) {
                logFollowCurrent =
                    !!logInfo &&
                    event.target.value ===
                        logInfo.current;

                renderLogMeta();
                updateLogActionButtons();

                loadLogContent(
                    event.target.value
                );
            }
        );

    document.getElementById("logSearchInput")
        .addEventListener(
            "input",
            renderLog
        );

    document.getElementById("logLevelSelect")
        .addEventListener(
            "change",
            renderLog
        );
}


function downloadConfiguration() {
    const anchor =
        document.createElement("a");

    anchor.href =
        "/api/config/devices/export";

    anchor.download =
        "saved_devices.json";

    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
}


async function restoreConfiguration() {
    const fileInput =
        document.getElementById(
            "configFileInput"
        );

    const file =
        fileInput.files[0];

    if (!file) {
        return;
    }

    if (!confirm(
            t("message.restoreConfirm")
        ))
    {
        return;
    }

    const restoreButton =
        document.getElementById(
            "restoreConfigButton"
        );

    restoreButton.disabled = true;

    try {
        const json =
            await file.text();

        const response =
            await fetch(
                "/api/config/devices/import",
                {
                    method: "POST",
                    headers: {
                        "Content-Type":
                            "application/json"
                    },
                    body: json
                }
            );

        if (!response.ok) {
            let message =
                t("message.restoreFailed");

            try {
                const body =
                    await response.json();

                if (body.message) {
                    message =
                        body.message;
                }
            }
            catch (_) {
                // Keep localized generic message.
            }

            throw new Error(message);
        }

        alert(
            t("message.restoreSuccess")
        );

        fileInput.value = "";

        const selectedFileName =
            document.getElementById(
                "selectedConfigFileName"
            );

        selectedFileName.textContent = "";
        selectedFileName.classList.add("hidden");

        restoreButton.classList.add("hidden");

        await loadDevices();
    }
    catch (error) {
        console.error(error);
        alert(error.message);
    }
    finally {
        restoreButton.disabled =
            fileInput.files.length === 0;
    }
}


function configureConfigurationBackup() {
    const fileInput =
        document.getElementById(
            "configFileInput"
        );

    const selectedFileName =
        document.getElementById(
            "selectedConfigFileName"
        );

    const restoreButton =
        document.getElementById(
            "restoreConfigButton"
        );

    document.getElementById(
        "downloadConfigButton"
    ).addEventListener(
        "click",
        downloadConfiguration
    );

    document.getElementById(
        "chooseConfigFileButton"
    ).addEventListener(
        "click",
        function () {
            fileInput.click();
        }
    );

    fileInput.addEventListener(
        "change",
        function () {
            const file =
                fileInput.files[0];

            if (file) {
                selectedFileName.textContent =
                    file.name;

                selectedFileName.classList.remove(
                    "hidden"
                );

                restoreButton.disabled = false;
                restoreButton.classList.remove(
                    "hidden"
                );
            }
            else {
                selectedFileName.textContent = "";

                selectedFileName.classList.add(
                    "hidden"
                );

                restoreButton.disabled = true;
                restoreButton.classList.add(
                    "hidden"
                );
            }
        }
    );

    restoreButton.addEventListener(
        "click",
        restoreConfiguration
    );
}


document.addEventListener(
    "DOMContentLoaded",
    async function () {
        try {
            await loadLanguageCatalog();
            await loadTranslations(
                currentLanguage
            );
        }
        catch (error) {
            console.error(error);

            currentLanguage = "en";

            try {
                await loadTranslations(
                    "en"
                );
            }
            catch (fallbackError) {
                console.error(
                    fallbackError
                );
            }
        }

        configureLanguageSelector();
        configureAuthenticationUi();
        configureMenu();
        configureNavigation();
        configureLogs();
        configureSystemPage();
        configureNetworkScan();
        configureDeviceEditor();
        configureTooltips();
        configureConfigurationBackup();
        configureWakeWizardConfig();

        try {
            const auth =
                await getAuthenticationStatus();

            authenticationRequired =
                auth.authRequired;

            /*
             * Brand-new / factory-reset WakeWizard:
             * no login is requested. The setup UI is shown directly.
             */
            if (
                !auth.authRequired ||
                auth.authenticated
            ) {
                showApplication();

                await initializeAuthenticatedApplication();
            }
            else {
                showLoginView();
            }
        }
        catch (error) {
            console.error(error);
            showLoginView();
        }
    }
);
