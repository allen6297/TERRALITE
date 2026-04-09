const tabViews = {
    blocks: [
        { id: 'block-data', label: 'Block Data' },
        { id: 'block-states', label: 'Block States' }
    ],
    items: [
        { id: 'item-data', label: 'Item Data' },
        { id: 'item-icons', label: 'Item Icons' }
    ],
    data: [
        { id: 'biome-data', label: 'Biome Data' }
    ],
    settings: [
        { id: 'world-data', label: 'Editor Settings' }
    ]
};

const viewToType = {
    'block-data': 'blocks',
    'block-states': 'block_states',
    'item-data': 'items',
    'biome-data': 'biomes'
};
const SETTINGS_FILE_PATH = '.content_editor/settings.json';
const shell = document.getElementById('shell');

const state = {
    bridge: null,
    activeTab: 'blocks',
    activeView: 'block-data',
    activeFilePanel: 'asset-browser',
    previousView: 'block-data',
    fileData: {
        'asset-browser': [],
        'model-files': [],
        'texture-files': []
    },
    fileFilters: {
        'asset-browser': '',
        'model-files': '',
        'texture-files': ''
    },
    fileNav: {
        'asset-browser': { parent: 'assets', subdir: '' },
        'model-files': { parent: 'blocks', subdir: '' },
        'texture-files': { parent: '', subdir: '' }
    },
    content: {
        blocks: { fileName: '', currentDocument: null },
        block_states: { fileName: '', currentDocument: null },
        items: { fileName: '', currentDocument: null },
        biomes: { fileName: '', currentDocument: null }
    }
};

const mainTabs = Array.from(document.querySelectorAll('[data-tab]'));
const subtabGroups = Array.from(document.querySelectorAll('[data-tab-group]'));
const subtabButtons = Array.from(document.querySelectorAll('[data-view].subtab-button'));
const panels = Array.from(document.querySelectorAll('[data-panel]'));

const fileDockPanels = Array.from(document.querySelectorAll('[data-file-dock-panel]'));
const fileDockActions = Array.from(document.querySelectorAll('.files-dock .subtab-button'));
const fileSearchInputs = Array.from(document.querySelectorAll('.file-search'));
const assetFileList = document.getElementById('asset-file-list');
const modelFileList = document.getElementById('model-file-list');
const textureFileList = document.getElementById('texture-file-list');
const parentTabHosts = {
    'asset-browser': document.getElementById('asset-parent-tabs'),
    'model-files': document.getElementById('model-parent-tabs'),
    'texture-files': document.getElementById('texture-parent-tabs')
};
const sidebarHosts = {
    'asset-browser': document.getElementById('asset-sidebar'),
    'model-files': document.getElementById('model-sidebar'),
    'texture-files': document.getElementById('texture-sidebar')
};

const statusPill = document.getElementById('status-pill');
const projectRoot = document.getElementById('project-root');

const workspaceTextureName = document.getElementById('workspace-texture-name');
const workspaceTexturePath = document.getElementById('workspace-texture-path');
const workspaceTexturePathCopy = document.getElementById('workspace-texture-path-copy');
const workspaceTextureSwatch = document.getElementById('workspace-texture-swatch');
const workspaceTextureImage = document.getElementById('workspace-texture-image');
const texturePreviewBack = document.getElementById('texture-preview-back');
const textureOpenEditor = document.getElementById('texture-open-editor');

const workspaceModelName = document.getElementById('workspace-model-name');
const workspaceModelPath = document.getElementById('workspace-model-path');
const workspaceModelPathCopy = document.getElementById('workspace-model-path-copy');
const workspaceModelJson = document.getElementById('workspace-model-json');
const modelPreviewBack = document.getElementById('model-preview-back');
const modelOpenEditor = document.getElementById('model-open-editor');
const projectSettingsTitle = document.getElementById('project-settings-title');
const settingsUi = {
    path: document.getElementById('project-settings-path'),
    meta: document.getElementById('project-settings-meta'),
    reloadButton: document.getElementById('project-settings-reload-button'),
    saveButton: document.getElementById('project-settings-save-button'),
    output: document.getElementById('project-settings-output'),
    themeSelect: document.getElementById('settings-theme-select'),
    textureEditorPathInput: document.getElementById('settings-texture-editor-path-input'),
    modelEditorPathInput: document.getElementById('settings-model-editor-path-input')
};
const settingsScriptFields = {
    build: {
        input: document.getElementById('settings-build-script-input'),
        runButton: document.getElementById('settings-build-script-run')
    },
    validate: {
        input: document.getElementById('settings-validate-script-input'),
        runButton: document.getElementById('settings-validate-script-run')
    },
    export: {
        input: document.getElementById('settings-export-script-input'),
        runButton: document.getElementById('settings-export-script-run')
    }
};

const DEFAULT_EDITOR_SETTINGS = {
    theme: 'system',
    editorPaths: {
        texture: '',
        model: ''
    },
    scripts: {
        build: '',
        validate: '',
        export: ''
    }
};

const editors = {
    blocks: {
        type: 'blocks',
        title: document.getElementById('block-selection-title'),
        saveButton: document.getElementById('block-save-button'),
        newButton: document.getElementById('block-new-button'),
        newRow: document.getElementById('block-new-file-row'),
        newInput: document.getElementById('block-new-file-input'),
        newConfirm: document.getElementById('block-new-file-confirm'),
        newCancel: document.getElementById('block-new-file-cancel'),
        previewImage: document.getElementById('block-preview-image'),
        previewColor: document.getElementById('block-preview-color'),
        previewEmpty: document.getElementById('block-preview-empty'),
        previewMeta: document.getElementById('block-preview-meta'),
        fields: {
            id: document.getElementById('block-id-input'),
            name: document.getElementById('block-name-input'),
            voxelSolid: document.getElementById('voxel-solid-input'),
            voxelTranslucent: document.getElementById('voxel-translucent-input'),
            voxelMaterial: document.getElementById('voxel-material-input'),
            renderModel: document.getElementById('render-model-input'),
            renderTextureAlbedo: document.getElementById('render-texture-albedo-input'),
            renderTextureNormal: document.getElementById('render-texture-normal-input'),
            renderTextureRoughness: document.getElementById('render-texture-roughness-input'),
            renderTextureEmissive: document.getElementById('render-texture-emissive-input'),
            renderColor: document.getElementById('render-color-input'),
            dropsText: document.getElementById('drops-input')
        }
    },
    block_states: {
        type: 'block_states',
        title: document.getElementById('bs-selection-title'),
        saveButton: document.getElementById('bs-save-button'),
        newButton: document.getElementById('bs-new-button'),
        newRow: document.getElementById('bs-new-file-row'),
        newInput: document.getElementById('bs-new-file-input'),
        newConfirm: document.getElementById('bs-new-file-confirm'),
        newCancel: document.getElementById('bs-new-file-cancel'),
        previewImage: document.getElementById('bs-preview-image'),
        previewColor: document.getElementById('bs-preview-color'),
        previewEmpty: document.getElementById('bs-preview-empty'),
        previewMeta: document.getElementById('bs-preview-meta'),
        fields: {
            rawJson: document.getElementById('bs-json-input')
        }
    },
    items: {
        type: 'items',
        title: document.getElementById('item-selection-title'),
        saveButton: document.getElementById('item-save-button'),
        newButton: document.getElementById('item-new-button'),
        newRow: document.getElementById('item-new-file-row'),
        newInput: document.getElementById('item-new-file-input'),
        newConfirm: document.getElementById('item-new-file-confirm'),
        newCancel: document.getElementById('item-new-file-cancel'),
        previewImage: document.getElementById('item-preview-image'),
        previewColor: document.getElementById('item-preview-color'),
        previewEmpty: document.getElementById('item-preview-empty'),
        previewMeta: document.getElementById('item-preview-meta'),
        fields: {
            id: document.getElementById('item-id-input'),
            name: document.getElementById('item-name-input'),
            stackSize: document.getElementById('stack-size-input'),
            placeableBlock: document.getElementById('placeable-block-input'),
            icon: document.getElementById('icon-input')
        }
    },
    biomes: {
        type: 'biomes',
        title: document.getElementById('biome-selection-title'),
        saveButton: document.getElementById('biome-save-button'),
        newButton: document.getElementById('biome-new-button'),
        newRow: document.getElementById('biome-new-file-row'),
        newInput: document.getElementById('biome-new-file-input'),
        newConfirm: document.getElementById('biome-new-file-confirm'),
        newCancel: document.getElementById('biome-new-file-cancel'),
        previewImage: document.getElementById('biome-preview-image'),
        previewColor: document.getElementById('biome-preview-color'),
        previewEmpty: document.getElementById('biome-preview-empty'),
        previewMeta: document.getElementById('biome-preview-meta'),
        fields: {
            id: document.getElementById('biome-id-input'),
            name: document.getElementById('biome-name-input'),
            priority: document.getElementById('biome-priority-input'),
            rarity: document.getElementById('biome-rarity-input'),
            terrainBaseHeight: document.getElementById('terrain-base-height-input'),
            terrainHeightVariation: document.getElementById('terrain-height-variation-input'),
            surfaceTop: document.getElementById('surface-top-input'),
            surfaceMiddle: document.getElementById('surface-middle-input'),
            surfaceMiddleDepth: document.getElementById('surface-middle-depth-input'),
            surfaceBase: document.getElementById('surface-base-input'),
            atmoSkyColor: document.getElementById('atmo-sky-input'),
            atmoFogColor: document.getElementById('atmo-fog-input'),
            atmoWaterColor: document.getElementById('atmo-water-input'),
            colorGrass: document.getElementById('color-grass-input'),
            colorFoliage: document.getElementById('color-foliage-input'),
            climateTempMin: document.getElementById('climate-temp-min-input'),
            climateTempMax: document.getElementById('climate-temp-max-input'),
            climateHumidityMin: document.getElementById('climate-humidity-min-input'),
            climateHumidityMax: document.getElementById('climate-humidity-max-input'),
            climateRainfallMin: document.getElementById('climate-rainfall-min-input'),
            climateRainfallMax: document.getElementById('climate-rainfall-max-input'),
            climateElevationMin: document.getElementById('climate-elevation-min-input'),
            climateElevationMax: document.getElementById('climate-elevation-max-input')
        }
    }
};

function currentType() {
    return viewToType[state.activeView] || null;
}

function clamp(value, min, max) {
    return Math.min(Math.max(value, min), max);
}

function selectWithin(container, selector, selectedElement) {
    if (!container) return;
    Array.from(container.querySelectorAll(selector)).forEach((item) => {
        item.classList.toggle('active', item === selectedElement);
    });
}

function setResizerValue(handle, value) {
    handle.setAttribute('aria-valuenow', String(Math.round(value)));
}

function setupHorizontalResizer(handle, buildConfig) {
    if (!handle) return;

    handle.addEventListener('pointerdown', (event) => {
        if (window.innerWidth <= 900) return;

        const config = buildConfig(handle);
        if (!config?.container || !config?.setValue || !config?.getNextValue) return;

        event.preventDefault();

        const bounds = config.container.getBoundingClientRect();
        const startValue = config.getStartValue(bounds);
        const pointerId = event.pointerId;

        handle.classList.add('is-dragging');
        handle.setPointerCapture(pointerId);
        document.body.style.cursor = 'col-resize';
        document.body.style.userSelect = 'none';

        const onPointerMove = (moveEvent) => {
            const nextValue = clamp(
                config.getNextValue(startValue, moveEvent.clientX - event.clientX, bounds),
                config.min(bounds),
                config.max(bounds)
            );
            config.setValue(nextValue);
            setResizerValue(handle, nextValue);
        };

        const stopDragging = () => {
            handle.classList.remove('is-dragging');
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
            handle.removeEventListener('pointermove', onPointerMove);
            handle.removeEventListener('pointerup', stopDragging);
            handle.removeEventListener('pointercancel', stopDragging);
        };

        handle.addEventListener('pointermove', onPointerMove);
        handle.addEventListener('pointerup', stopDragging);
        handle.addEventListener('pointercancel', stopDragging);
    });
}

function initResizablePanels() {
    setupHorizontalResizer(document.querySelector('.workspace-resizer'), () => ({
        container: document.querySelector('.workspace-grid'),
        getStartValue: (bounds) => {
            const width = parseFloat(getComputedStyle(shell).getPropertyValue('--files-dock-width'));
            return Number.isFinite(width) ? width : bounds.width * 0.3;
        },
        getNextValue: (startValue, delta) => startValue + delta,
        min: () => 280,
        max: (bounds) => Math.max(280, bounds.width - 420),
        setValue: (value) => {
            shell?.style.setProperty('--files-dock-width', `${value}px`);
        }
    }));

    document.querySelectorAll('.editor-resizer').forEach((handle) => {
        setupHorizontalResizer(handle, (resizerHandle) => {
            const layout = resizerHandle.closest('.editor-layout');
            return {
                container: layout,
                getStartValue: (bounds) => {
                    const width = parseFloat(getComputedStyle(layout).getPropertyValue('--editor-preview-width'));
                    return Number.isFinite(width) ? width : Math.min(320, bounds.width * 0.28);
                },
                getNextValue: (startValue, delta) => startValue - delta,
                min: () => 220,
                max: (bounds) => Math.max(220, bounds.width - 240),
                setValue: (value) => {
                    layout?.style.setProperty('--editor-preview-width', `${value}px`);
                }
            };
        });
    });
}

function openPreviewView(viewId) {
    if (state.activeView !== viewId) state.previousView = state.activeView;
    state.activeView = viewId;
    syncSubtabs();
    syncPanels();
}

function parseEditorSettings(content) {
    const parsed = {
        ...DEFAULT_EDITOR_SETTINGS,
        ...JSON.parse(content || '{}')
    };
    parsed.editorPaths = {
        ...DEFAULT_EDITOR_SETTINGS.editorPaths,
        ...(parsed.editorPaths || {})
    };
    parsed.scripts = {
        ...DEFAULT_EDITOR_SETTINGS.scripts,
        ...(parsed.scripts || {})
    };
    return parsed;
}

function buildEditorSettingsPayload() {
    return {
        theme: settingsUi.themeSelect?.value || 'system',
        editorPaths: {
            texture: settingsUi.textureEditorPathInput?.value || '',
            model: settingsUi.modelEditorPathInput?.value || ''
        },
        scripts: Object.fromEntries(
            Object.entries(settingsScriptFields).map(([key, value]) => [key, value.input?.value || ''])
        )
    };
}

async function loadProjectSettings({ setStatusMessage = true } = {}) {
    if (!state.bridge?.loadProjectTextFile) return false;
    const response = await state.bridge.loadProjectTextFile(SETTINGS_FILE_PATH);
    if (!response?.ok) {
        applyEditorSettings(DEFAULT_EDITOR_SETTINGS);
        if (projectSettingsTitle) projectSettingsTitle.textContent = 'Editor Settings';
        if (settingsUi.path) settingsUi.path.textContent = SETTINGS_FILE_PATH;
        if (settingsUi.meta) settingsUi.meta.textContent = 'Settings file not created yet. Save to create it.';
        if (settingsUi.output) settingsUi.output.textContent = '';
        if (setStatusMessage) setStatus('Editor settings not created yet');
        return false;
    }

    try {
        applyEditorSettings(parseEditorSettings(response.content));
    } catch (_error) {
        if (settingsUi.meta) settingsUi.meta.textContent = 'Settings JSON is invalid.';
        if (setStatusMessage) setStatus('Editor settings JSON is invalid', 'error');
        return false;
    }

    if (projectSettingsTitle) projectSettingsTitle.textContent = 'Editor Settings';
    if (settingsUi.path) settingsUi.path.textContent = response.path || SETTINGS_FILE_PATH;
    if (settingsUi.meta) settingsUi.meta.textContent = `Loaded ${response.path || SETTINGS_FILE_PATH}`;
    if (setStatusMessage) setStatus(`Loaded ${response.path || SETTINGS_FILE_PATH}`);
    return true;
}

async function saveProjectSettings() {
    if (!state.bridge?.saveProjectTextFile) return;
    const payload = buildEditorSettingsPayload();

    const response = await state.bridge.saveProjectTextFile(
        SETTINGS_FILE_PATH,
        JSON.stringify(payload, null, 2)
    );
    if (!response?.ok) {
        if (settingsUi.meta) settingsUi.meta.textContent = response?.message || 'Save failed.';
        setStatus(response?.message || 'Save failed', 'error');
        return;
    }

    if (settingsUi.path) settingsUi.path.textContent = response.path || SETTINGS_FILE_PATH;
    if (settingsUi.meta) settingsUi.meta.textContent = response.message || 'Saved.';
    setStatus(response.message || 'Saved editor settings');
    applyTheme(payload.theme);
    await loadAllProjectFiles();
}

function applyTheme(theme) {
    const resolvedTheme = theme === 'light' || theme === 'dark' ? theme : 'system';
    document.body.dataset.theme = resolvedTheme === 'system'
        ? (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light')
        : resolvedTheme;
}

function applyEditorSettings(settings) {
    if (settingsUi.themeSelect) settingsUi.themeSelect.value = settings.theme || 'system';
    if (settingsUi.textureEditorPathInput) settingsUi.textureEditorPathInput.value = settings.editorPaths?.texture || '';
    if (settingsUi.modelEditorPathInput) settingsUi.modelEditorPathInput.value = settings.editorPaths?.model || '';
    Object.entries(settingsScriptFields).forEach(([key, value]) => {
        if (value.input) value.input.value = settings.scripts?.[key] || '';
    });
    applyTheme(settings.theme || 'system');
}

async function runConfiguredProjectCommand(command, label) {
    if (!state.bridge?.runProjectCommand) return;
    if (settingsUi.output) settingsUi.output.textContent = `Running ${label}...\n`;
    const response = await state.bridge.runProjectCommand(command);
    const stdout = response?.stdout || '';
    const stderr = response?.stderr || '';
    if (settingsUi.output) {
        settingsUi.output.textContent = [stdout, stderr].filter(Boolean).join('\n').trim()
            || response?.message || 'No output.';
    }
    if (settingsUi.meta) settingsUi.meta.textContent = response?.message || `${label} finished.`;
    setStatus(response?.message || `${label} finished`, response?.ok ? 'neutral' : 'error');
}

function colorToCss(str) {
    if (!str) return '';
    const parts = str.split(',').map((segment) => Math.round(parseFloat(segment.trim()) * 255));
    if (parts.length < 3 || parts.some(Number.isNaN)) return '';
    return `rgb(${parts[0]}, ${parts[1]}, ${parts[2]})`;
}

function updateColorSwatch(input, swatchId) {
    const swatch = document.getElementById(swatchId);
    if (!swatch || !input) return;
    const css = colorToCss(input.value);
    swatch.style.background = css || 'rgba(255, 255, 255, 0.16)';
}

function bindColorInput(inputId, swatchId) {
    const input = document.getElementById(inputId);
    if (!input) return;
    input.addEventListener('input', () => updateColorSwatch(input, swatchId));
    updateColorSwatch(input, swatchId);
}

function setColorField(inputId, swatchId, value) {
    const input = document.getElementById(inputId);
    if (!input) return;
    input.value = value || '';
    updateColorSwatch(input, swatchId);
}

function setStatus(text, tone = 'neutral') {
    if (!statusPill) return;
    statusPill.textContent = text;
    statusPill.dataset.tone = tone;
}

function syncPanels() {
    panels.forEach((panel) => {
        panel.classList.toggle('active', panel.dataset.panel === state.activeView);
    });
}

function syncSubtabs() {
    subtabGroups.forEach((group) => {
        group.classList.toggle('active', group.dataset.tabGroup === state.activeTab);
    });

    subtabButtons.forEach((button) => {
        button.classList.toggle('active', button.dataset.view === state.activeView);
    });
}

function syncMainTabs() {
    mainTabs.forEach((button) => {
        button.classList.toggle('active', button.dataset.tab === state.activeTab);
    });
}

function syncFileDock() {
    fileDockPanels.forEach((panel) => {
        panel.classList.toggle('active', panel.dataset.fileDockPanel === state.activeFilePanel);
    });
}

function showPreviewImage(editor, url) {
    if (!editor) return;
    if (editor.previewImage) {
        if (url) {
            editor.previewImage.src = url;
            editor.previewImage.style.display = 'block';
        } else {
            editor.previewImage.removeAttribute('src');
            editor.previewImage.style.display = 'none';
        }
    }
    if (editor.previewColor) editor.previewColor.style.display = 'none';
    if (editor.previewEmpty) editor.previewEmpty.style.display = url ? 'none' : 'block';
}

function showPreviewColor(editor, color) {
    const css = colorToCss(color);
    if (!editor || !css) {
        showPreviewImage(editor, '');
        return;
    }
    if (editor.previewImage) editor.previewImage.style.display = 'none';
    if (editor.previewColor) {
        editor.previewColor.style.display = 'block';
        editor.previewColor.style.background = css;
    }
    if (editor.previewEmpty) editor.previewEmpty.style.display = 'none';
}

function renderEditorPreview(type, doc) {
    const editor = editors[type];
    if (!editor) return;

    if (!doc) {
        showPreviewImage(editor, '');
        if (editor.previewMeta) editor.previewMeta.textContent = '';
        return;
    }

    const url = doc.previewUrl || '';
    if (url) {
        showPreviewImage(editor, url);
    } else if (type === 'blocks' && doc.renderColor) {
        showPreviewColor(editor, doc.renderColor);
    } else {
        showPreviewImage(editor, '');
    }

    if (!editor.previewMeta) return;

    const lines = [`${type}: ${doc.id || '—'}`, `File: ${doc.fileName || '—'}`];
    if (type === 'blocks') {
        if (doc.voxelMaterial) lines.push(`Material: ${doc.voxelMaterial}`);
        if (doc.renderTextureAlbedo) lines.push(`Texture: ${doc.renderTextureAlbedo}`);
    } else if (type === 'items') {
        if (doc.placeableBlock) lines.push(`Places: ${doc.placeableBlock}`);
        if (doc.icon) lines.push(`Icon: ${doc.icon}`);
    } else if (type === 'biomes') {
        lines.push(`Priority ${doc.priority || '—'} · Rarity ${doc.rarity || '—'}`);
        if (doc.surfaceTop) lines.push(`Surface: ${doc.surfaceTop} / ${doc.surfaceMiddle || '—'} / ${doc.surfaceBase || '—'}`);
    } else if (type === 'block_states') {
        const variantCount = Object.keys(doc.variants || {}).length;
        lines.push(`Variants: ${variantCount}`);
    }
    editor.previewMeta.textContent = lines.join('\n');
}

function setEditorTitle(type, fileName) {
    const editor = editors[type];
    if (editor?.title) editor.title.textContent = fileName || 'No Selection';
}

function clearEditor(type) {
    const editor = editors[type];
    if (!editor) return;

    state.content[type].fileName = '';
    state.content[type].currentDocument = null;
    setEditorTitle(type, '');

    Object.entries(editor.fields || {}).forEach(([, field]) => {
        if (!field) return;
        if (field.type === 'checkbox') field.checked = false;
        else field.value = '';
    });

    if (type === 'blocks') {
        setColorField('render-color-input', 'render-color-swatch', '');
    } else if (type === 'biomes') {
        setColorField('atmo-sky-input', 'atmo-sky-swatch', '');
        setColorField('atmo-fog-input', 'atmo-fog-swatch', '');
        setColorField('atmo-water-input', 'atmo-water-swatch', '');
        setColorField('color-grass-input', 'color-grass-swatch', '');
        setColorField('color-foliage-input', 'color-foliage-swatch', '');
    }

    renderEditorPreview(type, null);
}

function populateEditor(doc) {
    const type = doc?.type;
    const editor = editors[type];
    if (!editor || !doc) return;

    state.content[type].fileName = doc.fileName || '';
    state.content[type].currentDocument = doc;
    setEditorTitle(type, doc.fileName || '');

    if (editor.fields.id) editor.fields.id.value = doc.id || '';
    if (editor.fields.name) editor.fields.name.value = doc.name || '';

    if (type === 'blocks') {
        editor.fields.voxelSolid.checked = !!doc.voxelSolid;
        editor.fields.voxelTranslucent.checked = !!doc.voxelTranslucent;
        editor.fields.voxelMaterial.value = doc.voxelMaterial || '';
        editor.fields.renderModel.value = doc.renderModel || '';
        editor.fields.renderTextureAlbedo.value = doc.renderTextureAlbedo || '';
        editor.fields.renderTextureNormal.value = doc.renderTextureNormal || '';
        editor.fields.renderTextureRoughness.value = doc.renderTextureRoughness || '';
        editor.fields.renderTextureEmissive.value = doc.renderTextureEmissive || '';
        editor.fields.dropsText.value = doc.dropsText || '';
        setColorField('render-color-input', 'render-color-swatch', doc.renderColor || '');
    } else if (type === 'items') {
        editor.fields.stackSize.value = doc.stackSize || '';
        editor.fields.placeableBlock.value = doc.placeableBlock || '';
        editor.fields.icon.value = doc.icon || '';
    } else if (type === 'biomes') {
        editor.fields.priority.value = doc.priority || '';
        editor.fields.rarity.value = doc.rarity || '';
        editor.fields.terrainBaseHeight.value = doc.terrainBaseHeight || '';
        editor.fields.terrainHeightVariation.value = doc.terrainHeightVariation || '';
        editor.fields.surfaceTop.value = doc.surfaceTop || '';
        editor.fields.surfaceMiddle.value = doc.surfaceMiddle || '';
        editor.fields.surfaceMiddleDepth.value = doc.surfaceMiddleDepth || '';
        editor.fields.surfaceBase.value = doc.surfaceBase || '';
        setColorField('atmo-sky-input', 'atmo-sky-swatch', doc.atmoSkyColor || '');
        setColorField('atmo-fog-input', 'atmo-fog-swatch', doc.atmoFogColor || '');
        setColorField('atmo-water-input', 'atmo-water-swatch', doc.atmoWaterColor || '');
        setColorField('color-grass-input', 'color-grass-swatch', doc.colorGrass || '');
        setColorField('color-foliage-input', 'color-foliage-swatch', doc.colorFoliage || '');
        editor.fields.climateTempMin.value = doc.climateTempMin || '';
        editor.fields.climateTempMax.value = doc.climateTempMax || '';
        editor.fields.climateHumidityMin.value = doc.climateHumidityMin || '';
        editor.fields.climateHumidityMax.value = doc.climateHumidityMax || '';
        editor.fields.climateRainfallMin.value = doc.climateRainfallMin || '';
        editor.fields.climateRainfallMax.value = doc.climateRainfallMax || '';
        editor.fields.climateElevationMin.value = doc.climateElevationMin || '';
        editor.fields.climateElevationMax.value = doc.climateElevationMax || '';
    } else if (type === 'block_states') {
        editor.fields.rawJson.value = doc.rawContent || '';
    }

    renderEditorPreview(type, doc);
}

function collectFields(type) {
    const editor = editors[type];
    if (!editor) return {};

    const fields = {
        id: editor.fields.id?.value || '',
        name: editor.fields.name?.value || ''
    };

    if (type === 'blocks') {
        fields.voxelSolid = !!editor.fields.voxelSolid?.checked;
        fields.voxelTranslucent = !!editor.fields.voxelTranslucent?.checked;
        fields.voxelMaterial = editor.fields.voxelMaterial?.value || '';
        fields.renderModel = editor.fields.renderModel?.value || '';
        fields.renderTextureAlbedo = editor.fields.renderTextureAlbedo?.value || '';
        fields.renderTextureNormal = editor.fields.renderTextureNormal?.value || '';
        fields.renderTextureRoughness = editor.fields.renderTextureRoughness?.value || '';
        fields.renderTextureEmissive = editor.fields.renderTextureEmissive?.value || '';
        fields.renderColor = editor.fields.renderColor?.value || '';
        fields.dropsText = editor.fields.dropsText?.value || '';
    } else if (type === 'items') {
        fields.stackSize = editor.fields.stackSize?.value || '';
        fields.placeableBlock = editor.fields.placeableBlock?.value || '';
        fields.icon = editor.fields.icon?.value || '';
    } else if (type === 'biomes') {
        fields.priority = editor.fields.priority?.value || '';
        fields.rarity = editor.fields.rarity?.value || '';
        fields.terrainBaseHeight = editor.fields.terrainBaseHeight?.value || '';
        fields.terrainHeightVariation = editor.fields.terrainHeightVariation?.value || '';
        fields.surfaceTop = editor.fields.surfaceTop?.value || '';
        fields.surfaceMiddle = editor.fields.surfaceMiddle?.value || '';
        fields.surfaceMiddleDepth = editor.fields.surfaceMiddleDepth?.value || '';
        fields.surfaceBase = editor.fields.surfaceBase?.value || '';
        fields.atmoSkyColor = editor.fields.atmoSkyColor?.value || '';
        fields.atmoFogColor = editor.fields.atmoFogColor?.value || '';
        fields.atmoWaterColor = editor.fields.atmoWaterColor?.value || '';
        fields.colorGrass = editor.fields.colorGrass?.value || '';
        fields.colorFoliage = editor.fields.colorFoliage?.value || '';
        fields.climateTempMin = editor.fields.climateTempMin?.value || '';
        fields.climateTempMax = editor.fields.climateTempMax?.value || '';
        fields.climateHumidityMin = editor.fields.climateHumidityMin?.value || '';
        fields.climateHumidityMax = editor.fields.climateHumidityMax?.value || '';
        fields.climateRainfallMin = editor.fields.climateRainfallMin?.value || '';
        fields.climateRainfallMax = editor.fields.climateRainfallMax?.value || '';
        fields.climateElevationMin = editor.fields.climateElevationMin?.value || '';
        fields.climateElevationMax = editor.fields.climateElevationMax?.value || '';
    }

    return fields;
}

async function loadContentDocument(type, fileName, { setStatusMessage = true } = {}) {
    if (!state.bridge || !fileName) return false;
    const result = await state.bridge.loadDocument(type, fileName);
    if (!result?.ok) {
        clearEditor(type);
        if (setStatusMessage) setStatus(result?.error || result?.message || `Failed to load ${fileName}`, 'error');
        return false;
    }

    if (type === 'block_states') {
        const rawResponse = await state.bridge.loadProjectTextFile(`data/states/blocks/${fileName}`);
        if (!rawResponse?.ok) {
            clearEditor(type);
            if (setStatusMessage) setStatus(rawResponse?.message || `Failed to load ${fileName}`, 'error');
            return false;
        }
        result.rawContent = rawResponse.content || '';
    }

    populateEditor(result);
    if (setStatusMessage) setStatus(`Loaded ${fileName}`);
    return true;
}

async function refreshContentType(type, { preserveSelection = true, setStatusMessage = true } = {}) {
    if (!state.bridge) return;
    const files = await state.bridge.listFiles(type);
    if (!files.length) {
        clearEditor(type);
        if (setStatusMessage) setStatus(`No ${type} files found`);
        return;
    }

    const selectedFile = preserveSelection && files.includes(state.content[type].fileName)
        ? state.content[type].fileName
        : files[0];

    await loadContentDocument(type, selectedFile, { setStatusMessage });
}

async function ensureLoadedForActiveView() {
    const type = currentType();
    if (!type) {
        if (state.activeView === 'world-data') {
            await loadProjectSettings({ setStatusMessage: false });
        }
        return;
    }

    if (!state.content[type].fileName) {
        await refreshContentType(type, { preserveSelection: false, setStatusMessage: false });
    }
}

async function activateView(viewId) {
    state.activeView = viewId;
    syncSubtabs();
    syncPanels();
    await ensureLoadedForActiveView();
}

async function activateTab(tabId) {
    state.activeTab = tabId;
    state.activeView = tabViews[tabId][0].id;
    syncMainTabs();
    syncSubtabs();
    syncPanels();
    await ensureLoadedForActiveView();
}

function activeSearchValue(panel) {
    const panelElement = document.querySelector(`[data-file-dock-panel="${panel}"]`);
    const input = panelElement ? panelElement.querySelector('.file-search') : null;
    return input ? input.value.trim().toLowerCase() : '';
}

function splitPathSegments(path) {
    return (path || '').split('/').filter(Boolean);
}

function entryParentKey(panel, entry) {
    if (panel === 'asset-browser') return entry.root || 'assets';
    return splitPathSegments(entry.path)[0] || '';
}

function entrySubdirPath(panel, entry, parentKey) {
    if (!entry?.path) return '';
    if (panel === 'asset-browser') {
        if ((entry.root || 'assets') !== parentKey) return '';
        const segments = splitPathSegments(entry.path);
        return segments.length > 1 ? segments.slice(0, -1).join('/') : '';
    }

    const segments = splitPathSegments(entry.path);
    if (!segments.length || segments[0] !== parentKey) return '';
    return segments.length > 2 ? segments.slice(1, -1).join('/') : '';
}

function derivePanelParents(panel, entries) {
    const seen = new Set();
    const parents = [];
    entries.forEach((entry) => {
        const key = entryParentKey(panel, entry);
        if (!key || seen.has(key)) return;
        seen.add(key);
        parents.push(key);
    });
    return parents;
}

function derivePanelSubdirs(panel, entries, parentKey) {
    const seen = new Set();
    const subdirs = [];
    entries.forEach((entry) => {
        const subdir = entrySubdirPath(panel, entry, parentKey);
        if (!subdir || seen.has(subdir)) return;
        seen.add(subdir);
        subdirs.push(subdir);
    });
    return subdirs.sort((a, b) => a.localeCompare(b));
}

function renderPanelParents(panel, entries) {
    const host = parentTabHosts[panel];
    if (!host) return;

    const parents = derivePanelParents(panel, entries);
    if (!parents.length) {
        host.replaceChildren();
        return;
    }

    if (!parents.includes(state.fileNav[panel].parent)) {
        state.fileNav[panel].parent = parents[0];
        state.fileNav[panel].subdir = '';
    }

    const buttons = parents.map((parentKey) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'file-parent-tab' + (state.fileNav[panel].parent === parentKey ? ' active' : '');
        button.textContent = parentKey;
        button.addEventListener('click', () => {
            state.fileNav[panel].parent = parentKey;
            state.fileNav[panel].subdir = '';
            renderProjectFiles(panel, state.fileData[panel]);
        });
        return button;
    });

    host.replaceChildren(...buttons);
}

function renderPanelSidebar(panel, entries) {
    const host = sidebarHosts[panel];
    if (!host) return;

    const heading = host.querySelector('.file-sidebar-head');
    host.replaceChildren();
    if (heading) host.appendChild(heading);

    const parentKey = state.fileNav[panel].parent;
    const subdirs = derivePanelSubdirs(panel, entries, parentKey);
    const availableKeys = [''].concat(subdirs);
    if (!availableKeys.includes(state.fileNav[panel].subdir)) {
        state.fileNav[panel].subdir = '';
    }

    const allButton = document.createElement('button');
    allButton.type = 'button';
    allButton.className = 'file-node' + (!state.fileNav[panel].subdir ? ' active' : '');
    allButton.textContent = 'All';
    allButton.addEventListener('click', () => {
        state.fileNav[panel].subdir = '';
        renderProjectFiles(panel, state.fileData[panel]);
    });
    host.appendChild(allButton);

    subdirs.forEach((subdir) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'file-node' + (state.fileNav[panel].subdir === subdir ? ' active' : '');
        button.textContent = subdir;
        button.addEventListener('click', () => {
            state.fileNav[panel].subdir = subdir;
            renderProjectFiles(panel, state.fileData[panel]);
        });
        host.appendChild(button);
    });
}

function parseAssetContentEntry(entry) {
    const path = entry?.path || '';
    if (!path.endsWith('.json')) return null;

    if (path.startsWith('blocks/')) {
        return { type: 'blocks', fileName: path.slice('blocks/'.length), view: 'block-data', tab: 'blocks' };
    }
    if (path.startsWith('items/')) {
        return { type: 'items', fileName: path.slice('items/'.length), view: 'item-data', tab: 'items' };
    }
    if (path.startsWith('biomes/')) {
        return { type: 'biomes', fileName: path.slice('biomes/'.length), view: 'biome-data', tab: 'data' };
    }
    if (path.startsWith('states/blocks/')) {
        return { type: 'block_states', fileName: path.slice('states/blocks/'.length), view: 'block-states', tab: 'blocks' };
    }
    if (path === 'project.json') {
        return { type: null, fileName: path, view: 'world-data', tab: 'data' };
    }
    return null;
}

function renderFileEntry(entry, active = false, onClick = null) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'file-entry' + (active ? ' active' : '');

    const name = document.createElement('span');
    name.className = 'file-entry-name';
    name.textContent = entry.name || entry.path || '';
    button.appendChild(name);

    const meta = document.createElement('span');
    meta.className = 'file-entry-meta';
    meta.textContent = entry.kind || '';
    button.appendChild(meta);

    if (onClick) {
        button.addEventListener('click', () => onClick(button, entry));
    }

    return button;
}

function updateModelPreview(entry, content = '') {
    const modelName = entry?.name || '';
    const modelPath = entry?.path || '';

    if (workspaceModelName) workspaceModelName.textContent = modelName;
    if (workspaceModelPath) workspaceModelPath.textContent = modelPath;
    if (workspaceModelPathCopy) workspaceModelPathCopy.textContent = modelPath;
    if (workspaceModelJson) workspaceModelJson.textContent = content || '';
}

async function openModelPreview(button, entry) {
    selectWithin(modelFileList, '.file-entry', button);
    let content = '';
    if (state.bridge?.loadProjectTextFile && entry?.path) {
        const response = await state.bridge.loadProjectTextFile(`assets/models/${entry.path}`);
        if (response?.ok) content = response.content || '';
        else content = response?.message ? `Failed to load model file.\n${response.message}` : 'Failed to load model file.';
    }

    updateModelPreview(entry, content);
    openPreviewView('model-preview');
}

async function openModelPreviewFromAssetEntry(button, entry) {
    selectWithin(assetFileList, '.file-entry', button);
    const normalizedPath = (entry?.path || '').replace(/^models\//, '');
    let content = '';
    if (state.bridge?.loadProjectTextFile && normalizedPath) {
        const response = await state.bridge.loadProjectTextFile(`assets/models/${normalizedPath}`);
        if (response?.ok) content = response.content || '';
        else content = response?.message ? `Failed to load model file.\n${response.message}` : 'Failed to load model file.';
    }

    updateModelPreview({
        name: entry?.name || '',
        path: normalizedPath
    }, content);
    openPreviewView('model-preview');
}

function updateTexturePreview(tile) {
    if (!tile) return;
    const textureName = tile.dataset.textureName || tile.textContent.trim();
    const texturePath = tile.dataset.texturePath || '';
    const texturePreviewUrl = tile.dataset.texturePreviewUrl || '';

    if (workspaceTextureName) workspaceTextureName.textContent = textureName;
    if (workspaceTexturePath) workspaceTexturePath.textContent = texturePath;
    if (workspaceTexturePathCopy) workspaceTexturePathCopy.textContent = texturePath;
    if (workspaceTextureSwatch) {
        if (texturePreviewUrl) {
            workspaceTextureSwatch.classList.add('has-image');
            if (workspaceTextureImage) {
                workspaceTextureImage.src = texturePreviewUrl;
                workspaceTextureImage.alt = textureName;
            }
        } else {
            workspaceTextureSwatch.classList.remove('has-image');
            if (workspaceTextureImage) {
                workspaceTextureImage.removeAttribute('src');
                workspaceTextureImage.alt = 'Selected texture preview';
            }
        }
    }
}

function openTexturePreviewFromAssetEntry(button, entry) {
    selectWithin(assetFileList, '.file-entry', button);
    const normalizedPath = (entry?.path || '')
        .replace(/^textures\//, '')
        .replace(/^ui\//, 'ui/');

    updateTexturePreview({
        dataset: {
            textureName: entry?.name || '',
            texturePath: normalizedPath,
            texturePreviewUrl: entry?.previewUrl || ''
        },
        textContent: entry?.name || ''
    });

    openPreviewView('texture-preview');
}

async function openCurrentPreviewInEditor(editorType, pathText, button, defaultLabel) {
    const relativePath = pathText?.textContent?.trim();
    if (!state.bridge?.openProjectFileInExternalEditor || !relativePath || !button) return;

    button.disabled = true;
    button.textContent = 'Opening...';
    try {
        const response = await state.bridge.openProjectFileInExternalEditor(relativePath, editorType);
        button.textContent = response?.ok ? 'Opened' : 'Launch Failed';
        if (!response?.ok) setStatus(response?.message || 'Launch failed', 'error');
    } catch (_error) {
        button.textContent = 'Launch Failed';
        setStatus('Launch failed', 'error');
    }

    window.setTimeout(() => {
        button.disabled = false;
        button.textContent = defaultLabel;
    }, 1200);
}

function renderTextureTile(entry, active = false) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'texture-tile' + (active ? ' active' : '');
    button.dataset.textureName = entry.name || '';
    button.dataset.texturePath = entry.path || '';
    if (entry.previewUrl) button.dataset.texturePreviewUrl = entry.previewUrl;

    const thumb = document.createElement('span');
    thumb.className = 'texture-thumb';

    const thumbImage = document.createElement('img');
    thumbImage.alt = entry.name || 'Texture thumbnail';
    if (entry.previewUrl) {
        thumbImage.src = entry.previewUrl;
        thumb.classList.add('has-image');
    }
    thumb.appendChild(thumbImage);
    button.appendChild(thumb);

    const label = document.createElement('span');
    label.className = 'texture-label';
    label.textContent = entry.name || '';
    button.appendChild(label);

    button.addEventListener('click', () => {
        selectWithin(textureFileList, '.texture-tile', button);
        updateTexturePreview(button);
        openPreviewView('texture-preview');
    });

    return button;
}

async function openAssetEntry(button, entry) {
    selectWithin(assetFileList, '.file-entry', button);

    const contentEntry = parseAssetContentEntry(entry);
    if (contentEntry) {
        state.activeTab = contentEntry.tab;
        state.activeView = contentEntry.view;
        syncMainTabs();
        syncSubtabs();
        syncPanels();

        if (contentEntry.type) {
            await loadContentDocument(contentEntry.type, contentEntry.fileName);
        } else if (contentEntry.view === 'world-data') {
            await loadProjectSettings();
        }
        return;
    }

    if (entry?.root === 'assets' && entry?.path?.startsWith('models/') && entry?.path?.endsWith('.json')) {
        await openModelPreviewFromAssetEntry(button, entry);
        return;
    }

    if (entry?.root === 'assets' && entry?.previewUrl) {
        openTexturePreviewFromAssetEntry(button, entry);
    }
}

function renderProjectFiles(panel, entries) {
    state.fileData[panel] = entries || [];
    renderPanelParents(panel, state.fileData[panel]);
    renderPanelSidebar(panel, state.fileData[panel]);

    const parentKey = state.fileNav[panel].parent;
    const subdirFilter = state.fileNav[panel].subdir;
    const folderFilter = (state.fileFilters[panel] || '').toLowerCase();
    const searchFilter = activeSearchValue(panel);
    const filteredEntries = state.fileData[panel].filter((entry) => {
        const path = (entry.path || '').toLowerCase();
        const name = (entry.name || '').toLowerCase();
        const parentMatch = !parentKey || entryParentKey(panel, entry) === parentKey;
        const entrySubdir = entrySubdirPath(panel, entry, parentKey).toLowerCase();
        const subdirMatch = !subdirFilter || entrySubdir === subdirFilter.toLowerCase() || entrySubdir.startsWith(`${subdirFilter.toLowerCase()}/`);
        const folderMatch = !folderFilter || path.startsWith(folderFilter);
        const searchMatch = !searchFilter || path.includes(searchFilter) || name.includes(searchFilter);
        return parentMatch && subdirMatch && folderMatch && searchMatch;
    });

    if (panel === 'asset-browser' && assetFileList) {
        assetFileList.replaceChildren(...filteredEntries.map((entry) => renderFileEntry(entry, false, openAssetEntry)));
        return;
    }

    if (panel === 'model-files' && modelFileList) {
        modelFileList.replaceChildren(...filteredEntries.map((entry) => renderFileEntry(entry, false, openModelPreview)));
        return;
    }

    if (panel === 'texture-files' && textureFileList) {
        const tiles = filteredEntries.map((entry) => renderTextureTile(entry, false));
        textureFileList.replaceChildren(...tiles);
        updateTexturePreview(textureFileList.querySelector('.texture-tile.active'));
    }
}

async function loadProjectFiles(panel) {
    if (!state.bridge) return;
    const entries = await state.bridge.listProjectFiles(panel);
    renderProjectFiles(panel, entries || []);
}

async function loadAllProjectFiles() {
    await loadProjectFiles('asset-browser');
    await loadProjectFiles('model-files');
    await loadProjectFiles('texture-files');
}

function showNewFileRow(type) {
    const editor = editors[type];
    if (!editor?.newRow || !editor.newInput) return;
    editor.newRow.style.display = 'flex';
    editor.newInput.value = '';
    editor.newInput.focus();
}

function hideNewFileRow(type) {
    const editor = editors[type];
    if (!editor?.newRow || !editor.newInput) return;
    editor.newRow.style.display = 'none';
    editor.newInput.value = '';
}

async function confirmNewFile(type) {
    const editor = editors[type];
    if (!editor?.newInput || !state.bridge) return;
    const raw = editor.newInput.value.trim();
    if (!raw) {
        editor.newInput.focus();
        return;
    }

    const fileName = raw.endsWith('.json') ? raw : `${raw}.json`;
    const result = await state.bridge.createDocument(type, fileName);
    if (!result?.ok) {
        setStatus(result?.error || result?.message || 'Create failed', 'error');
        return;
    }

    hideNewFileRow(type);
    populateEditor(result);
    setStatus(result.message || `Created ${fileName}`);
    await loadAllProjectFiles();
}

async function saveCurrentType(type) {
    if (!state.bridge) return;
    const fileName = state.content[type]?.fileName;
    if (!fileName) {
        setStatus('No file selected', 'error');
        return;
    }

    if (type === 'block_states') {
        const rawJson = editors.block_states?.fields.rawJson?.value || '';
        const saveResult = await state.bridge.saveProjectTextFile(`data/states/blocks/${fileName}`, rawJson);
        if (!saveResult?.ok) {
            setStatus(saveResult?.message || 'Save failed', 'error');
            return;
        }

        await loadContentDocument(type, fileName, { setStatusMessage: false });
        setStatus(saveResult.message || `Saved ${fileName}`);
        await loadAllProjectFiles();
        return;
    }

    const result = await state.bridge.saveDocument(type, fileName, collectFields(type));
    if (!result?.ok) {
        setStatus(result?.error || result?.message || 'Save failed', 'error');
        return;
    }

    populateEditor(result);
    setStatus(result.message || `Saved ${fileName}`);
    await loadAllProjectFiles();
}

function bindEditorButtons(type) {
    const editor = editors[type];
    if (!editor) return;

    if (editor.saveButton) {
        editor.saveButton.addEventListener('click', () => {
            saveCurrentType(type);
        });
    }

    if (editor.newButton) {
        editor.newButton.addEventListener('click', () => {
            showNewFileRow(type);
        });
    }

    if (editor.newConfirm) {
        editor.newConfirm.addEventListener('click', () => {
            confirmNewFile(type);
        });
    }

    if (editor.newCancel) {
        editor.newCancel.addEventListener('click', () => {
            hideNewFileRow(type);
        });
    }

    if (editor.newInput) {
        editor.newInput.addEventListener('keydown', (event) => {
            if (event.key === 'Enter') confirmNewFile(type);
            if (event.key === 'Escape') hideNewFileRow(type);
        });
    }
}

function bindEvents() {
    mainTabs.forEach((button) => {
        button.addEventListener('click', () => {
            activateTab(button.dataset.tab);
        });
    });

    subtabButtons.forEach((button) => {
        button.addEventListener('click', () => {
            activateView(button.dataset.view);
        });
    });

    fileSearchInputs.forEach((input) => {
        input.addEventListener('input', () => {
            const panel = input.closest('[data-file-dock-panel]');
            if (!panel) return;
            renderProjectFiles(panel.dataset.fileDockPanel, state.fileData[panel.dataset.fileDockPanel]);
        });
    });

    fileDockActions.forEach((button) => {
        button.addEventListener('click', async () => {
            button.classList.add('active');
            await loadAllProjectFiles();
            const type = currentType();
            if (type) await refreshContentType(type, { preserveSelection: true, setStatusMessage: false });
            window.setTimeout(() => button.classList.remove('active'), 160);
        });
    });

    if (texturePreviewBack) {
        texturePreviewBack.addEventListener('click', () => {
            activateView(state.previousView || tabViews[state.activeTab][0].id);
        });
    }

    if (modelPreviewBack) {
        modelPreviewBack.addEventListener('click', () => {
            activateView(state.previousView || tabViews[state.activeTab][0].id);
        });
    }

    if (textureOpenEditor) {
        textureOpenEditor.addEventListener('click', () => {
            openCurrentPreviewInEditor('texture', workspaceTexturePath, textureOpenEditor, 'Open in Aseprite');
        });
    }

    if (modelOpenEditor) {
        modelOpenEditor.addEventListener('click', () => {
            openCurrentPreviewInEditor('model', workspaceModelPath, modelOpenEditor, 'Open in Blockbench');
        });
    }

    if (settingsUi.reloadButton) {
        settingsUi.reloadButton.addEventListener('click', () => {
            loadProjectSettings();
        });
    }

    if (settingsUi.saveButton) {
        settingsUi.saveButton.addEventListener('click', () => {
            saveProjectSettings();
        });
    }

    if (settingsUi.themeSelect) {
        settingsUi.themeSelect.addEventListener('change', () => {
            applyTheme(settingsUi.themeSelect.value);
        });
    }

    Object.entries(settingsScriptFields).forEach(([key, value]) => {
        if (value.runButton) {
            value.runButton.addEventListener('click', () => {
                runConfiguredProjectCommand(value.input?.value || '', key);
            });
        }
    });

    Object.keys(editors).forEach(bindEditorButtons);

    bindColorInput('render-color-input', 'render-color-swatch');
    bindColorInput('atmo-sky-input', 'atmo-sky-swatch');
    bindColorInput('atmo-fog-input', 'atmo-fog-swatch');
    bindColorInput('atmo-water-input', 'atmo-water-swatch');
    bindColorInput('color-grass-input', 'color-grass-swatch');
    bindColorInput('color-foliage-input', 'color-foliage-swatch');
}

async function boot(bridge) {
    state.bridge = bridge;
    if (projectRoot && bridge.projectRootPath) {
        projectRoot.textContent = await bridge.projectRootPath();
    }

    Object.keys(editors).forEach(clearEditor);
    initResizablePanels();
    bindEvents();
    await activateTab(state.activeTab);
    syncFileDock();
    await loadAllProjectFiles();
    setStatus('Ready');
}

window.addEventListener('DOMContentLoaded', () => {
    setStatus('Connecting…');
    new QWebChannel(qt.webChannelTransport, (channel) => {
        boot(channel.objects.editorBridge);
    });
});
