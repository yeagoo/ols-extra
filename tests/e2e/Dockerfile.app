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

# Copy OLS configuration for app testing
COPY tests/e2e/conf/httpd_config_app.conf /usr/local/lsws/conf/httpd_config.conf
COPY tests/e2e/conf/vhconf_app.conf /usr/local/lsws/conf/vhosts/apps/vhconf.conf

# Create document root with subdirectories for each PHP app (Req 13.6)
RUN mkdir -p /var/www/vhosts/apps/html/wordpress \
             /var/www/vhosts/apps/html/nextcloud \
             /var/www/vhosts/apps/html/drupal \
             /var/www/vhosts/apps/html/laravel \
    && echo "<h1>App Stack OK</h1>" > /var/www/vhosts/apps/html/index.html \
    && chown -R nobody:nogroup /var/www/vhosts/apps

EXPOSE 8088
