FROM litespeedtech/openlitespeed:latest

# =============================================================================
# PHP Application Testing Dockerfile
#
# Based on litespeedtech/openlitespeed with LSPHP 8.1+ and required extensions.
# Used by docker-compose.yml for WordPress, Nextcloud, Drupal, Laravel testing.
#
# Requirements: 13.2, 13.3, 13.6
# =============================================================================

# Install LSPHP 8.1 with required extensions
RUN apt-get update && apt-get install -y --no-install-recommends \
    lsphp81 \
    lsphp81-common \
    lsphp81-mysql \
    lsphp81-curl \
    lsphp81-gd \
    lsphp81-intl \
    lsphp81-mbstring \
    lsphp81-xml \
    lsphp81-zip \
    curl \
    unzip \
    wget \
    mariadb-client \
    && rm -rf /var/lib/apt/lists/*

# Ensure lsphp81 is the default PHP binary
RUN ln -sf /usr/local/lsws/lsphp81/bin/php /usr/local/bin/php

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
