/* Shared two-theme preference and sidebar controls for every MiniFS page. */
(function () {
  'use strict';

  const THEME_KEY = 'minifs-theme';
  const SIDEBAR_KEY = 'minifs-sidebar-collapsed';
  const root = document.documentElement;

  function readPreference(key, fallback) {
    try {
      const value = window.localStorage.getItem(key);
      return value === null ? fallback : value;
    } catch (_) {
      return fallback;
    }
  }

  function writePreference(key, value) {
    try {
      window.localStorage.setItem(key, value);
    } catch (_) {
      /* The site remains usable when storage is unavailable. */
    }
  }

  function setTheme(theme) {
    const selected = theme === 'light' ? 'light' : 'dark';

    root.dataset.theme = selected;
    root.style.colorScheme = selected;

    const button = document.getElementById('themeToggle');
    if (!button) return;

    const light = selected === 'light';

    button.setAttribute(
      'aria-label',
      light ? 'Switch to dark theme' : 'Switch to light theme'
    );

    button.setAttribute('aria-pressed', String(light));
    button.title = light ? 'Switch to dark theme' : 'Switch to light theme';
    button.dataset.nextTheme = light ? 'dark' : 'light';

    button.innerHTML =
      '<span class="theme-icon" aria-hidden="true">' +
      (light ? '☼' : '☾') +
      '</span><span class="theme-label">' +
      (light ? 'Light theme' : 'Dark theme') +
      '</span>';
  }

  function addThemeToggle() {
    let button = document.getElementById('themeToggle');
    if (button) return button;

    button = document.createElement('button');
    button.type = 'button';
    button.id = 'themeToggle';
    button.className = 'theme-toggle';

    button.addEventListener('click', function () {
      const next = root.dataset.theme === 'light' ? 'dark' : 'light';

      writePreference(THEME_KEY, next);
      setTheme(next);
    });

    const topActions = document.querySelector('.top-actions');
    const wasmTopbar = document.querySelector('.shell .topbar');

    if (topActions) {
      const github = topActions.querySelector('.github-button');
      topActions.insertBefore(button, github || null);
    } else if (wasmTopbar) {
      wasmTopbar.appendChild(button);
    }

    return button;
  }

  function addSidebarCollapse() {
    const sidebar = document.getElementById('sidebar');

    if (!sidebar || document.getElementById('sidebarCollapse')) return;

    const button = document.createElement('button');

    button.type = 'button';
    button.id = 'sidebarCollapse';
    button.className = 'sidebar-collapse';
    button.setAttribute('aria-controls', 'sidebar');

    button.addEventListener('click', function () {
      const collapsed = !sidebar.classList.contains('is-collapsed');

      sidebar.classList.toggle('is-collapsed', collapsed);

      button.setAttribute('aria-expanded', String(!collapsed));
      button.setAttribute(
        'aria-label',
        collapsed ? 'Expand sidebar' : 'Collapse sidebar'
      );

      button.title = collapsed ? 'Expand sidebar' : 'Collapse sidebar';
      button.textContent = collapsed ? '»' : '«';

      writePreference(SIDEBAR_KEY, String(collapsed));
    });

    const brand = sidebar.querySelector('.brand');

    sidebar.insertBefore(
      button,
      brand || sidebar.firstChild
    );

    const collapsed =
      readPreference(SIDEBAR_KEY, 'false') === 'true';

    sidebar.classList.toggle('is-collapsed', collapsed);

    button.setAttribute('aria-expanded', String(!collapsed));
    button.setAttribute(
      'aria-label',
      collapsed ? 'Expand sidebar' : 'Collapse sidebar'
    );

    button.title = collapsed ? 'Expand sidebar' : 'Collapse sidebar';
    button.textContent = collapsed ? '»' : '«';

    const names = {
      simulator: 'Simulator',
      learn: 'Command guide',
      practice: 'Practice missions',
      concepts: 'Concepts & Quiz'
    };

    sidebar.querySelectorAll('.nav-item').forEach(function (item) {
      const name =
        names[item.dataset.tab] || item.textContent.trim();

      item.dataset.tooltip = name;
      item.title = name;

      if (!item.hasAttribute('aria-label')) {
        item.setAttribute('aria-label', name);
      }
    });
  }

  function loadEnhancementScripts() {
    const load = (name, next) => {
      if (
        document.querySelector(
          'script[data-minifs-script="' + name + '"]'
        )
      ) {
        if (next) next();
        return;
      }

      const script = document.createElement('script');

      script.src = name;
      script.dataset.minifsScript = name;

      script.onload = function () {
        if (next) next();
      };

      script.onerror = function () {
        console.error('MiniFS enhancement could not load:', name);
      };

      document.body.appendChild(script);
    };

    load('syntax-highlight.js', function () {
      load('ui-enhancements.js');
    });
  }

  function init() {
    const saved = readPreference(THEME_KEY, 'dark');

    setTheme(saved);
    addThemeToggle();
    setTheme(saved);
    addSidebarCollapse();
    loadEnhancementScripts();

    const mobileMenu = document.getElementById('mobileMenu');
    const sidebar = document.getElementById('sidebar');

    if (mobileMenu && sidebar) {
      mobileMenu.setAttribute('aria-controls', 'sidebar');
      mobileMenu.setAttribute('aria-expanded', 'false');

      mobileMenu.addEventListener('click', function () {
        window.setTimeout(function () {
          const open = sidebar.classList.contains('open');

          mobileMenu.setAttribute('aria-expanded', String(open));
          mobileMenu.setAttribute(
            'aria-label',
            open ? 'Close navigation' : 'Open navigation'
          );
        }, 0);
      });
    }
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