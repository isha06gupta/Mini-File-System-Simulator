
/* Small shared usability enhancements; filesystem logic stays in its existing engine. */
(function () {
  'use strict';

  const $ = (selector, root) =>
    (root || document).querySelector(selector);

  const $$ = (selector, root) =>
    Array.from((root || document).querySelectorAll(selector));

  function safeStore(key, value) {
    try {
      localStorage.setItem(key, value);
    } catch (_) {
      /* Optional preference. */
    }
  }

  function announce(message) {
    let region = $('#uiEnhancementStatus');

    if (!region) {
      region = document.createElement('div');
      region.id = 'uiEnhancementStatus';
      region.className = 'ui-enhancement-status';
      region.setAttribute('role', 'status');
      region.setAttribute('aria-live', 'polite');

      document.body.appendChild(region);
    }

    region.textContent = message;
    region.classList.add('is-visible');

    window.clearTimeout(announce.timer);

    announce.timer = window.setTimeout(
      () => region.classList.remove('is-visible'),
      2400
    );
  }

  function makeButton(id, label, text, parent) {
    if ($('#' + id) || !parent) {
      return $('#' + id);
    }

    const button = document.createElement('button');

    button.type = 'button';
    button.id = id;
    button.className = 'subtle-button enhancement-button';
    button.textContent = text;
    button.title = label;
    button.setAttribute('aria-label', label);

    parent.appendChild(button);

    return button;
  }

  function loadEnhancementStyles() {
    if (
      document.querySelector(
        'link[data-minifs-styles="ui-enhancements"]'
      )
    ) {
      return;
    }

    const link = document.createElement('link');

    link.rel = 'stylesheet';
    link.href = 'ui-enhancements.css';
    link.dataset.minifsStyles = 'ui-enhancements';

    document.head.appendChild(link);
  }

  function addTreeNameTooltips() {
    const tree = $('#fileTree');

    if (!tree) return;

    const update = () => {
      $$('.tree-row', tree).forEach((row) => {
        const name = $('.tree-name', row);

        if (!name) return;

        const fullName = name.textContent.trim();

        if (!fullName) return;

        name.title = fullName;

        const type = $('.tree-type', row)?.textContent.trim() || 'item';

        row.setAttribute(
          'aria-label',
          fullName + ', ' + type
        );
      });
    };

    update();

    const observer = new MutationObserver(update);

    observer.observe(tree, {
      childList: true,
      subtree: true
    });
  }

  function addCommandGuideSearch() {
    const host = $('#commandCards');

    if (!host || $('#commandGuideSearch')) return;

    const toolbar = document.createElement('div');

    toolbar.className = 'command-guide-tools';

    const label = document.createElement('label');

    label.htmlFor = 'commandGuideSearch';
    label.textContent = 'Find a command';

    const input = document.createElement('input');

    input.id = 'commandGuideSearch';
    input.type = 'search';
    input.placeholder = 'Search syntax, purpose, category, or concept…';
    input.autocomplete = 'off';

    const count = document.createElement('span');

    count.id = 'commandGuideCount';
    count.className = 'tool-count';
    count.setAttribute('aria-live', 'polite');

    toolbar.append(label, input, count);

    host.parentNode.insertBefore(toolbar, host);

    const update = () => {
      const query = input.value.trim().toLowerCase();
      const cards = $$('.command-card', host);
      let visible = 0;

      cards.forEach((card) => {
        const match =
          !query || card.textContent.toLowerCase().includes(query);

        card.hidden = !match;

        if (match) visible++;
      });

      count.textContent = query
        ? visible + ' of ' + cards.length + ' commands'
        : cards.length + ' commands available';
    };

    input.addEventListener('input', update);

    const observer = new MutationObserver(update);

    observer.observe(host, {
      childList: true,
      subtree: true
    });

    update();
  }

  async function copyText(text) {
    if (!text.trim()) {
      announce('There is no output to copy.');
      return;
    }

    try {
      await navigator.clipboard.writeText(text);

      announce('Terminal output copied.');
    } catch (_) {
      const field = document.createElement('textarea');

      field.value = text;
      field.readOnly = true;
      field.className = 'clipboard-fallback';

      document.body.appendChild(field);

      field.select();

      const success = document.execCommand('copy');

      field.remove();

      announce(
        success
          ? 'Terminal output copied.'
          : 'Clipboard access was blocked by the browser.'
      );
    }
  }

  function addTerminalActions() {
    const output = $('#terminalOutput');
    const actions = $('.terminal-actions');

    if (!output || !actions) return;

    const copy = makeButton(
      'copyTerminalOutput',
      'Copy all terminal output',
      'Copy output',
      actions
    );

    if (copy) {
      copy.addEventListener('click', () => {
        copyText(output.innerText || output.textContent || '');
      });
    }

    const exportButton = makeButton(
      'exportTerminalHistory',
      'Export terminal output as a text file',
      'Export history',
      actions
    );

    if (exportButton) {
      exportButton.addEventListener('click', () => {
        const text = output.innerText || output.textContent || '';

        if (!text.trim()) {
          announce('There is no terminal history to export.');
          return;
        }

        const blob = new Blob(
          [text.trimEnd() + '\n'],
          { type: 'text/plain;charset=utf-8' }
        );

        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');

        link.href = url;
        link.download = 'minifs-terminal-history.txt';

        document.body.appendChild(link);

        link.click();
        link.remove();

        window.setTimeout(() => URL.revokeObjectURL(url), 1000);

        announce('Terminal history download started.');
      });
    }
  }

  function addPanelExpandControls() {
    const panels = [
      ['.explorer-panel', 'file explorer'],
      ['.terminal-panel', 'terminal'],
      ['.inspector-panel', 'learning panel']
    ];

    panels.forEach(([selector, label]) => {
      const panel = $(selector);
      const header = panel && $('.panel-header', panel);

      if (!panel || !header || $('.panel-expand', header)) {
        return;
      }

      const button = document.createElement('button');

      button.type = 'button';
      button.className = 'subtle-button panel-expand';
      button.textContent = 'Expand';
      button.dataset.panelLabel = label;
      button.setAttribute('aria-label', 'Expand ' + label);
      button.title = 'Expand ' + label;
      button.setAttribute('aria-pressed', 'false');

      header.appendChild(button);

      button.addEventListener('click', () => {
        const wasExpanded = panel.classList.contains('is-expanded');

        /*
         * Only one panel may be expanded at a time.
         * Reset the previous panel and its own control label.
         */
        $$('.is-expanded').forEach((item) => {
          item.classList.remove('is-expanded');

          const control = $('.panel-expand', item);

          if (!control) return;

          const itemLabel = control.dataset.panelLabel || 'panel';

          control.textContent = 'Expand';
          control.setAttribute(
            'aria-label',
            'Expand ' + itemLabel
          );
          control.title = 'Expand ' + itemLabel;
          control.setAttribute('aria-pressed', 'false');
        });

        const workspace = $('.workspace-grid');

        if (workspace) {
          workspace.classList.remove('explorer-expanded');
        }

        /*
         * Clicking the currently expanded panel restores it.
         * Explorer expansion changes the workspace grid rather than
         * floating the Explorer over the terminal.
         */
        if (!wasExpanded) {
          panel.classList.add('is-expanded');

          if (selector === '.explorer-panel' && workspace) {
            workspace.classList.add('explorer-expanded');
          }

          button.textContent = 'Restore';
          button.setAttribute('aria-label', 'Restore ' + label);
          button.title = 'Restore ' + label;
          button.setAttribute('aria-pressed', 'true');
        }
      });
    });
  }

  function addWorkspaceSplitter() {
    const workspace = $('.workspace-grid');
    const explorer = $('.explorer-panel', workspace || document);
    const right = $('.right-column', workspace || document);

    if (!workspace || !explorer || !right || $('#workspaceSplitter')) {
      return;
    }

    const splitter = document.createElement('div');

    splitter.id = 'workspaceSplitter';
    splitter.className = 'workspace-splitter';
    splitter.setAttribute('role', 'separator');
    splitter.setAttribute('aria-orientation', 'vertical');
    splitter.setAttribute(
      'aria-label',
      'Resize file explorer and terminal panels'
    );
    splitter.setAttribute('tabindex', '0');

    explorer.after(splitter);

    workspace.classList.add('workspace-resizable');

    const saved = Number.parseInt(
      (() => {
        try {
          return localStorage.getItem('minifs-explorer-width') || '42';
        } catch (_) {
          return '42';
        }
      })(),
      10
    );

    const clamp = (value) => Math.max(32, Math.min(58, value));

    const setWidth = (value) => {
      const width = clamp(value);

      workspace.style.setProperty(
        '--explorer-width',
        width + '%'
      );

      splitter.setAttribute('aria-valuemin', '32');
      splitter.setAttribute('aria-valuemax', '58');
      splitter.setAttribute(
        'aria-valuenow',
        String(Math.round(width))
      );

      safeStore(
        'minifs-explorer-width',
        String(Math.round(width))
      );
    };

    setWidth(Number.isFinite(saved) ? saved : 42);

    let dragging = false;

    splitter.addEventListener('pointerdown', (event) => {
      if (window.matchMedia('(max-width: 980px)').matches) {
        return;
      }

      dragging = true;

      if (splitter.setPointerCapture) {
        splitter.setPointerCapture(event.pointerId);
      }

      document.body.classList.add('is-resizing-workspace');

      event.preventDefault();
    });

    splitter.addEventListener('pointermove', (event) => {
      if (!dragging) return;

      const rect = workspace.getBoundingClientRect();

      if (rect.width) {
        setWidth(
          ((event.clientX - rect.left) / rect.width) * 100
        );
      }
    });

    const stop = () => {
      dragging = false;

      document.body.classList.remove('is-resizing-workspace');
    };

    splitter.addEventListener('pointerup', stop);
    splitter.addEventListener('pointercancel', stop);

    splitter.addEventListener('keydown', (event) => {
      if (
        event.key !== 'ArrowLeft' &&
        event.key !== 'ArrowRight'
      ) {
        return;
      }

      event.preventDefault();

      const current =
        Number.parseInt(
          getComputedStyle(workspace).getPropertyValue(
            '--explorer-width'
          ),
          10
        ) || 42;

      setWidth(
        current + (event.key === 'ArrowRight' ? 2 : -2)
      );
    });
  }

  function keyboardShortcuts() {
    document.addEventListener('keydown', (event) => {
      const target = event.target;

      const typing = target && (
        target.matches('input, textarea, select') ||
        target.isContentEditable
      );

      if (
        (event.ctrlKey || event.metaKey) &&
        event.key.toLowerCase() === 'k'
      ) {
        event.preventDefault();

        const input = $('#commandInput');

        if (input && !input.disabled) {
          input.focus();
        }
      } else if (
        (event.ctrlKey || event.metaKey) &&
        event.shiftKey &&
        event.key.toLowerCase() === 'l'
      ) {
        event.preventDefault();

        const clear = $('#clearTerminalBtn') || $('#clearBtn');

        if (clear) clear.click();
      } else if (
        !typing &&
        event.key === '/' &&
        !event.ctrlKey &&
        !event.metaKey &&
        !event.altKey
      ) {
        const search = $('#commandGuideSearch');
        const guide = $('#learnView');

        if (
          search &&
          guide &&
          !guide.classList.contains('hidden')
        ) {
          event.preventDefault();
          search.focus();
        }
      } else if (event.key === 'Escape') {
        const sidebar = $('#sidebar');
        const mobileMenu = $('#mobileMenu');

        if (sidebar) {
          sidebar.classList.remove('open');
        }

        if (mobileMenu) {
          mobileMenu.setAttribute('aria-expanded', 'false');
        }
      }
    });
  }

  function init() {
    loadEnhancementStyles();
    addTreeNameTooltips();
    addCommandGuideSearch();
    addTerminalActions();
    addPanelExpandControls();
    addWorkspaceSplitter();
    keyboardShortcuts();
  }

  if (document.readyState === 'loading') {
    document.addEventListener(
      'DOMContentLoaded',
      init,
      { once: true }
    );
  } else {
    init();
  }
})();
