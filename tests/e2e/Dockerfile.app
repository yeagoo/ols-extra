FROM litespeedtech/openlitespeed:latest

# =============================================================================
# PHP Application Testing Dockerfile
#
# Based on litespeedtech/openlitespeed with LSPHP 8.1+ and required extensions.
# Used by docker-compose.yml for WordPress, Nextcloud, Drupal, Laravel testing.
#
# Requirements: 13.2, 13.3, 13.6
# =============================================================================

# Install additional packages needed for PHP app testing.
# NOTE: The base image already ships LSPHP with gd, mbstring, xml, zip
# compiled in. Only separate extension packages (mysql, curl, intl) and
# system utilities need to be installed explicitly.
RUN apt-get update && apt-get install -y --no-install-recommends \
    lsphp81-mysql \
    lsphp81-curl \
    lsphp81-intl \
    curl \
    unzip \
    wget \
    mariadb-client \
    && rm -rf /var/lib/apt/lists/*

# Ensure lsphp81 is the default PHP binary (may already exist in base image)
RUN ln -sf /usr/local/lsws/lsphp81/bin/php /usr/local/bin/php 2>/dev/null || true

# Copy the compiled ols_htaccess module (Req 13.3)
COPY build/ols_htaccess.so /usr/local/lsws/modules/ols_htaccess.so

# Create document root with subdirectories for each PHP app (Req 13.6)
RUN mkdir -p /var/www/vhosts/localhost/html/wordpress \
             /var/www/vhosts/localhost/html/nextcloud \
             /var/www/vhosts/localhost/html/drupal \
             /var/www/vhosts/localhost/html/laravel \
    && echo "<h1>App Stack OK</h1>" > /var/www/vhosts/localhost/html/index.html \
    && chown -R nobody:nogroup /var/www/vhosts/localhost

# Patch the default vhost config to enable .htaccess support and PHP
RUN VHCONF="/usr/local/lsws/conf/vhosts/localhost/vhconf.conf" && \
    if [ -f "$VHCONF" ]; then \
      sed -i 's/allowOverride.*/allowOverride             255/' "$VHCONF" || true; \
      grep -q 'allowOverride' "$VHCONF" || echo 'allowOverride 255' >> "$VHCONF"; \
    fi

# Add module loading to httpd_config.conf
RUN CONF="/usr/local/lsws/conf/httpd_config.conf" && \
    if ! grep -q 'ols_htaccess' "$CONF" 2>/dev/null; then \
      echo '' >> "$CONF"; \
      echo 'module ols_htaccess {' >> "$CONF"; \
      echo '  ls_enabled              1' >> "$CONF"; \
      echo '}' >> "$CONF"; \
    fi

EXPOSE 8088
