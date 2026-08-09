#!/bin/bash
# Extract all sites from Wave Host into static HTML files
# These files can be served by GitHub Pages or any static server.

SITES_DIR="data/sites"
OUTPUT_DIR="docs/sites"

mkdir -p "$OUTPUT_DIR"

echo "🌊 Exporting Wave sites to static HTML..."

for file in "$SITES_DIR"/*.html; do
    if [ -f "$file" ]; then
        subdomain=$(basename "$file" .html)
        cp "$file" "$OUTPUT_DIR/$subdomain.html"
        echo "  ✅ $subdomain.w.END.d → $OUTPUT_DIR/$subdomain.html"
    fi
done

# Also export the platform homepage
cp site/platform.html "$OUTPUT_DIR/index.html" 2>/dev/null || \
    echo "  ⚠️  platform.html not found, skipping"

echo ""
echo "✅ Static export complete."
echo "   Files are in $OUTPUT_DIR/"
echo "   Push to GitHub and enable Pages on /docs folder."
