#!/usr/bin/env python3
"""
Simple CPM Registry Server for demonstration
HTTP server that handles package publishing, searching, and downloading
"""

import os
import sys
import json
import tarfile
import tempfile
import shutil
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import cgi
import sqlite3
from datetime import datetime

# Configuration
REGISTRY_HOST = '0.0.0.0'
REGISTRY_PORT = 8080
DB_FILE = 'cpm_registry.db'
PACKAGES_DIR = 'packages'

class CPMRegistryHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        self.setup_database()
        super().__init__(*args, **kwargs)
    
    def setup_database(self):
        """Initialize SQLite database for package metadata"""
        if not os.path.exists(DB_FILE):
            conn = sqlite3.connect(DB_FILE)
            cursor = conn.cursor()
            
            cursor.execute('''
                CREATE TABLE packages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT NOT NULL,
                    version TEXT NOT NULL,
                    description TEXT,
                    author TEXT,
                    homepage TEXT,
                    repository TEXT,
                    license TEXT,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    downloads INTEGER DEFAULT 0,
                    UNIQUE(name, version)
                )
            ''')
            
            # Insert some demo packages
            demo_packages = [
                ('libmath', '1.0.0', 'Mathematical library for C', 'Math Team', 'https://github.com/mathteam/libmath', '', 'MIT'),
                ('libmath', '1.1.0', 'Mathematical library for C (updated)', 'Math Team', 'https://github.com/mathteam/libmath', '', 'MIT'),
                ('libutils', '2.0.1', 'Utility functions for C development', 'Utils Team', 'https://github.com/utilsteam/libutils', '', 'Apache-2.0'),
                ('libnetwork', '0.9.5', 'Network programming utilities', 'Network Group', 'https://github.com/netgroup/libnetwork', '', 'BSD-3-Clause'),
            ]
            
            for pkg in demo_packages:
                cursor.execute('''
                    INSERT OR IGNORE INTO packages 
                    (name, version, description, author, homepage, repository, license) 
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', pkg)
            
            conn.commit()
            conn.close()
    
    def do_GET(self):
        """Handle GET requests for package search and download"""
        parsed_path = urlparse(self.path)
        path = parsed_path.path
        query_params = parse_qs(parsed_path.query)
        
        if path == '/packages/search':
            self.handle_search(query_params)
        elif path.startswith('/packages/') and '/versions' in path:
            package_name = path.split('/')[2]
            self.handle_get_versions(package_name)
        elif path.startswith('/packages/') and len(path.split('/')) >= 4:
            # Package download: /packages/{name}/{version}
            parts = path.split('/')
            package_name = parts[2]
            version = parts[3]
            self.handle_download(package_name, version)
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not Found')
    
    def do_POST(self):
        """Handle POST requests for package publishing"""
        if self.path == '/packages/upload':
            self.handle_upload()
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not Found')
    
    def handle_search(self, query_params):
        """Handle package search requests"""
        query = query_params.get('q', [''])[0]
        
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        
        if query:
            cursor.execute('''
                SELECT DISTINCT name, MAX(version) as latest_version, description, author, homepage, downloads
                FROM packages 
                WHERE name LIKE ? OR description LIKE ?
                GROUP BY name
                ORDER BY downloads DESC, name
            ''', (f'%{query}%', f'%{query}%'))
        else:
            cursor.execute('''
                SELECT DISTINCT name, MAX(version) as latest_version, description, author, homepage, downloads
                FROM packages
                GROUP BY name
                ORDER BY downloads DESC, name
            ''')
        
        results = cursor.fetchall()
        conn.close()
        
        packages = []
        for row in results:
            packages.append({
                'name': row[0],
                'version': row[1],
                'description': row[2],
                'author': row[3],
                'homepage': row[4],
                'downloads': row[5]
            })
        
        response = {
            'query': query,
            'packages': packages,
            'total': len(packages)
        }
        
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(response, indent=2).encode())
    
    def handle_get_versions(self, package_name):
        """Handle requests for package version list"""
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT version, created_at 
            FROM packages 
            WHERE name = ?
            ORDER BY created_at DESC
        ''', (package_name,))
        
        results = cursor.fetchall()
        conn.close()
        
        versions = [{'version': row[0], 'created_at': row[1]} for row in results]
        
        response = {
            'package': package_name,
            'versions': versions
        }
        
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(response, indent=2).encode())
    
    def handle_download(self, package_name, version):
        """Handle package download requests"""
        # Increment download counter
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute('''
            UPDATE packages SET downloads = downloads + 1 
            WHERE name = ? AND version = ?
        ''', (package_name, version))
        conn.commit()
        conn.close()
        
        # Return package metadata (in real implementation, would return tarball)
        package_info = {
            'name': package_name,
            'version': version,
            'download_url': f'/packages/{package_name}/{version}/archive.tar.gz',
            'status': 'available'
        }
        
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(package_info, indent=2).encode())
    
    def handle_upload(self):
        """Handle package upload requests"""
        try:
            # Parse multipart form data
            content_type = self.headers.get('Content-Type', '')
            if not content_type.startswith('multipart/form-data'):
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b'Content-Type must be multipart/form-data')
                return
            
            # Simple form parsing (in production, use proper multipart parser)
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            
            # Extract package info from form data (simplified)
            # In real implementation, would properly parse the multipart data
            
            # Mock successful upload
            response = {
                'status': 'success',
                'message': 'Package uploaded successfully',
                'timestamp': datetime.now().isoformat()
            }
            
            self.send_response(201)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(response, indent=2).encode())
            
        except Exception as e:
            self.send_response(500)
            self.end_headers()
            self.wfile.write(f'Upload failed: {str(e)}'.encode())
    
    def log_message(self, format, *args):
        """Override to provide cleaner logging"""
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {format % args}")

def main():
    """Start the CPM registry server"""
    # Create packages directory
    os.makedirs(PACKAGES_DIR, exist_ok=True)
    
    # Start server
    server_address = (REGISTRY_HOST, REGISTRY_PORT)
    httpd = HTTPServer(server_address, CPMRegistryHandler)
    
    print(f"CPM Registry Server starting...")
    print(f"Listening on http://{REGISTRY_HOST}:{REGISTRY_PORT}")
    print(f"Package database: {DB_FILE}")
    print(f"Packages directory: {PACKAGES_DIR}")
    print("\nEndpoints:")
    print("  GET  /packages/search?q=<query>")
    print("  GET  /packages/<name>/versions")
    print("  GET  /packages/<name>/<version>")
    print("  POST /packages/upload")
    print("\nPress Ctrl+C to stop the server")
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.shutdown()

if __name__ == '__main__':
    main()