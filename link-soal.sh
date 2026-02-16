wget https://pocongcyber77.github.io/ADMIN_OPREC_ASLAB_SISOP_25-26/menu.svg

grep -oE '<!--\s*[^-]+\s*-->' menu.svg | sed -E 's/<!--\s*|\s*-->//g' | tr -d '\n' ; echo
