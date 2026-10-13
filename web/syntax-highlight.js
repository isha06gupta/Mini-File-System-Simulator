
/* Lightweight, safe syntax highlighting for C, commands, and inline code. */
(function () {
  'use strict';

  const C_KEYWORDS = new Set(
    (
      'auto break case char const continue default do double else enum extern ' +
      'float for goto inline int long register restrict return short signed ' +
      'sizeof static struct switch typedef union unsigned void volatile while ' +
      '_Bool _Complex _Imaginary bool true false NULL'
    ).split(/\s+/)
  );

  const COMMANDS = new Set(
    (
      'help clear pwd ls tree cd mkdir rmdir touch cat read write append rm ' +
      'cp mv rename find grep stat du quota chmod chown ln symlink readlink ' +
      'history undo redo snapshot restore restoreversion save load fsck format ' +
      'reset exit echo whoami date df mount umount man'
    ).split(/\s+/)
  );

  function token(kind, value) {
    const span = document.createElement('span');

    span.className = 'syntax-' + kind;
    span.textContent = value;

    return span;
  }

  function appendText(parent, kind, value) {
    if (!value) return;

    parent.appendChild(
      kind
        ? token(kind, value)
        : document.createTextNode(value)
    );
  }

  function highlightC(source, target) {
    let i = 0;
    let plainStart = 0;

    const flush = function (end) {
      appendText(target, '', source.slice(plainStart, end));
    };

    while (i < source.length) {
      const start = i;
      const ch = source[i];
      const next = source[i + 1];
      let kind = '';

      const lineStart = source.lastIndexOf('\n', i - 1) + 1;
      const onlyWhitespaceBefore =
        source.slice(lineStart, i).trim() === '';

      if (ch === '#' && onlyWhitespaceBefore) {
        while (i < source.length && source[i] !== '\n') i++;
        kind = 'preprocessor';
      } else if (ch === '/' && next === '/') {
        i += 2;

        while (i < source.length && source[i] !== '\n') {
          i++;
        }

        kind = 'comment';
      } else if (ch === '/' && next === '*') {
        i += 2;

        while (
          i < source.length &&
          !(source[i] === '*' && source[i + 1] === '/')
        ) {
          i++;
        }

        i = Math.min(source.length, i + 2);
        kind = 'comment';
      } else if (ch === '"' || ch === "'") {
        const quote = ch;

        i++;

        while (i < source.length) {
          if (source[i] === '\\') {
            i += 2;
            continue;
          }

          if (source[i] === quote) {
            i++;
            break;
          }

          i++;
        }

        kind = 'string';
      } else if (
        /[0-9]/.test(ch) ||
        (ch === '.' && /[0-9]/.test(next || ''))
      ) {
        i++;

        while (
          i < source.length &&
          /[A-Za-z0-9_.]/.test(source[i])
        ) {
          i++;
        }

        kind = 'number';
      } else if (/[A-Za-z_]/.test(ch)) {
        i++;

        while (
          i < source.length &&
          /[A-Za-z0-9_]/.test(source[i])
        ) {
          i++;
        }

        const word = source.slice(start, i);
        const whitespaceLength = source.slice(i).match(/^\s*/)[0].length;
        const nextNonWhitespace = i + whitespaceLength;

        kind = C_KEYWORDS.has(word)
          ? 'keyword'
          : source[nextNonWhitespace] === '('
            ? 'function'
            : '';
      } else if ('{}[]();,.'.includes(ch)) {
        i++;
        kind = 'punctuation';
      } else if ('+-*/%=!<>|&^~?:'.includes(ch)) {
        i++;

        while (
          i < source.length &&
          /[=<>|&+]/.test(source[i])
        ) {
          i++;
        }

        kind = 'operator';
      } else {
        i++;
        continue;
      }

      flush(start);
      appendText(target, kind, source.slice(start, i));
      plainStart = i;
    }

    flush(source.length);
  }

  function highlightCommand(source, target) {
    const trimmedStart = source.search(/\S/);

    if (trimmedStart < 0) {
      target.textContent = source;
      return;
    }

    appendText(target, '', source.slice(0, trimmedStart));

    let i = trimmedStart;
    let firstWord = true;

    while (i < source.length) {
      const start = i;
      const ch = source[i];

      if (/\s/.test(ch)) {
        while (i < source.length && /\s/.test(source[i])) {
          i++;
        }

        appendText(target, '', source.slice(start, i));
        continue;
      }

      if (ch === '"' || ch === "'") {
        const quote = ch;

        i++;

        while (i < source.length) {
          if (source[i] === '\\') {
            i += 2;
            continue;
          }

          if (source[i] === quote) {
            i++;
            break;
          }

          i++;
        }

        appendText(target, 'string', source.slice(start, i));
        firstWord = false;
        continue;
      }

      while (i < source.length && !/\s/.test(source[i])) {
        i++;
      }

      const part = source.slice(start, i);
      let kind = '';

      if (firstWord && (part === '/›' || part === '$')) {
        kind = 'prompt';
      } else if (firstWord) {
        const command = part.toLowerCase();

        kind = COMMANDS.has(command)
          ? 'command'
          : 'argument';

        firstWord = false;
      } else if (/^--?[A-Za-z][\w-]*$/.test(part)) {
        kind = 'flag';
      } else if (
        /^(\/|\.\.\/|\.\/|~\/)/.test(part) ||
        /\.[A-Za-z0-9]{1,8}$/.test(part)
      ) {
        kind = 'path';
      } else if (/^-?\d+(?:\.\d+)?$/.test(part)) {
        kind = 'number';
      } else {
        kind = 'argument';
      }

      appendText(target, kind, part);
    }
  }

  function detectMode(element) {
    if (
      element.matches(
        '.source-card pre, .source-card header code, ' +
        'pre.language-c, pre.c-code, code.language-c'
      )
    ) {
      return 'c';
    }

    if (
      element.matches(
        '.terminal-line.command, .line.command, .command-example, ' +
        '.card-example, code.language-shell, code.language-bash'
      )
    ) {
      return 'command';
    }

    if (element.tagName === 'PRE') {
      const content = element.textContent;

      if (
        /\b(struct|typedef|#include|int\s+main|void\s+\w+\s*\()/.test(
          content
        )
      ) {
        return 'c';
      }

      if (
        /^(?:\$\s*)?(mkdir|touch|write|append|tree|ls|cd|pwd|help)\b/m.test(
          content
        )
      ) {
        return 'command';
      }
    }

    if (element.tagName === 'CODE') {
      const content = element.textContent.trim();
      const first = content.split(/\s+/)[0].toLowerCase();

      if (COMMANDS.has(first)) return 'command';

      if (
        C_KEYWORDS.has(first) ||
        /\w+\s*\(/.test(content)
      ) {
        return 'c';
      }
    }

    return '';
  }

  function highlight(element) {
    if (!element || element.dataset.syntaxHighlighted === 'true') {
      return;
    }

    if (element.closest('.syntax-highlighted')) return;

    const mode = detectMode(element);
    if (!mode) return;

    const source = element.textContent;
    const fragment = document.createDocumentFragment();

    if (mode === 'c') {
      highlightC(source, fragment);
    } else {
      highlightCommand(source, fragment);
    }

    element.replaceChildren(fragment);
    element.dataset.syntaxHighlighted = 'true';
    element.classList.add('syntax-highlighted');

    if (mode === 'c') {
      element.classList.add('syntax-c');
    } else {
      element.classList.add('syntax-command');
    }
  }

  function scan(root) {
    if (
      root.nodeType === Node.ELEMENT_NODE &&
      root.matches(
        'pre, code, .terminal-line.command, .line.command, ' +
        '.command-example, .card-example'
      )
    ) {
      highlight(root);
    }

    if (root.querySelectorAll) {
      root
        .querySelectorAll(
          'pre, code, .terminal-line.command, .line.command, ' +
          '.command-example, .card-example'
        )
        .forEach(highlight);
    }
  }

  function init() {
    scan(document);

    const observer = new MutationObserver(function (records) {
      records.forEach(function (record) {
        record.addedNodes.forEach(function (node) {
          if (node.nodeType === Node.ELEMENT_NODE) {
            scan(node);
          }
        });
      });
    });

    observer.observe(document.body, {
      childList: true,
      subtree: true
    });
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
