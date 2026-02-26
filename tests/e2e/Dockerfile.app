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
    bzip2 \
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

# Patch the default vhost config to enable .htaccess support, rewrite, and PHP.
# We overwrite the base image's vhconf.conf entirely to ensure a known-good state.
RUN VHCONF="/usr/local/lsws/conf/vhosts/localhost/vhconf.conf" && \
    mkdir -p "$(dirname "$VHCONF")" && \
    cat > "$VHCONF" <<'VHEOF'
docRoot                   /var/www/vhosts/localhost/html
enableRewrite             1
allowOverride             255
autoIndex                 0

rewrite  {
  enable                  1
}

context / {
  allowBrowse             1
  location                /var/www/vhosts/localhost/html
  rewrite  {
    enable                1
  }
}

scripthandler {
  add                     lsapi:lsphp php
}
VHEOF

# Ensure the httpd_config has an lsphp extprocessor and scripthandler.
# The base image may use a different PHP version — we patch to use lsphp81
# which has our mysql/curl/intl extensions installed.
RUN CONF="/usr/local/lsws/conf/httpd_config.conf" && \
    if ! grep -q 'ols_htaccess' "$CONF" 2>/dev/null; then \
      echo '' >> "$CONF"; \
      echo 'module ols_htaccess {' >> "$CONF"; \
      echo '  ls_enabled              1' >> "$CONF"; \
      echo '}' >> "$CONF"; \
    fi && \
    if ! grep -q 'extprocessor lsphp' "$CONF" 2>/dev/null; then \
      echo '' >> "$CONF"; \
      echo 'extprocessor lsphp {' >> "$CONF"; \
      echo '  type                    lsapi' >> "$CONF"; \
      echo '  address                 uds://tmp/lshttpd/lsphp.sock' >> "$CONF"; \
      echo '  maxConns                10' >> "$CONF"; \
      echo '  env                     PHP_LSAPI_CHILDREN=10' >> "$CONF"; \
      echo '  env                     LSAPI_AVOID_FORK=200M' >> "$CONF"; \
      echo '  initTimeout             60' >> "$CONF"; \
      echo '  retryTimeout            0' >> "$CONF"; \
      echo '  persistConn             1' >> "$CONF"; \
      echo '  respBuffer              0' >> "$CONF"; \
      echo '  autoStart               2' >> "$CONF"; \
      echo '  path                    /usr/local/lsws/lsphp81/bin/lsphp' >> "$CONF"; \
      echo '  backlog                 100' >> "$CONF"; \
      echo '  instances               1' >> "$CONF"; \
      echo '}' >> "$CONF"; \
    fi && \
    if ! grep -q 'scripthandler' "$CONF" 2>/dev/null; then \
      echo '' >> "$CONF"; \
      echo 'scripthandler {' >> "$CONF"; \
      echo '  add                     lsapi:lsphp php' >> "$CONF"; \
      echo '}' >> "$CONF"; \
    fi

# Also ensure the base image's existing lsphp extprocessor points to lsphp81
# (base image may have lsphp83 or other version as default)
RUN CONF="/usr/local/lsws/conf/httpd_config.conf" && \
    sed -i 's|/usr/local/lsws/lsphp[0-9]*/bin/lsphp|/usr/local/lsws/lsphp81/bin/lsphp|g' "$CONF" || true

EXPOSE 8088
