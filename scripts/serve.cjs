const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const webRoot = path.resolve(__dirname, '..', 'web');
const host = process.env.HOST || '127.0.0.1';
const port = Number(process.env.PORT || 8000);
const types = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon'
};

const server = http.createServer((req, res) => {
  if (!['GET', 'HEAD'].includes(req.method)) {
    res.writeHead(405, { 'Content-Type': 'text/plain; charset=utf-8', Allow: 'GET, HEAD' });
    return res.end('Method not allowed');
  }
  let pathname;
  try { pathname = decodeURIComponent(new URL(req.url, `http://${req.headers.host || 'localhost'}`).pathname); }
  catch { res.writeHead(400); return res.end('Bad request'); }
  if (pathname.includes('\\') || pathname.includes('\0')) { res.writeHead(400); return res.end('Bad request'); }
  if (pathname === '/') pathname = '/index.html';
  const filePath = path.resolve(webRoot, `.${pathname}`);
  if (filePath !== webRoot && !filePath.startsWith(webRoot + path.sep)) {
    res.writeHead(403, { 'Content-Type': 'text/plain; charset=utf-8' });
    return res.end('Forbidden');
  }
  fs.stat(filePath, (statError, stat) => {
    if (statError || !stat.isFile()) {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      return res.end('Not found');
    }
    res.writeHead(200, {
      'Content-Type': types[path.extname(filePath).toLowerCase()] || 'application/octet-stream',
      'Content-Length': stat.size,
      'X-Content-Type-Options': 'nosniff',
      'Referrer-Policy': 'no-referrer',
      'Cache-Control': 'no-cache'
    });
    if (req.method === 'HEAD') return res.end();
    fs.createReadStream(filePath).pipe(res);
  });
});

server.listen(port, host, () => {
  console.log(`MiniFS Learning Lab: http://${host}:${port}`);
  console.log('Static files served from web/. Press Ctrl+C to stop.');
});
server.on('error', error => {
  console.error(`Could not start server: ${error.message}`);
  process.exitCode = 1;
});
