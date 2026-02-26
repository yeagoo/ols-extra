FROM litespeedtech/openlitespeed:latest
# The base image provides listeners on ports 80/443 via vhTemplate,
# docRoot at /var/www/vhosts/localhost/html/, built-in .htaccess via
# autoLoadHtaccess, and lsphp via fcgi-bin symlinks.
# We add: lsphp81 extensions, ols_htaccess module, app directories.
# docker-compose.yml maps host 8088 -> container 80.
# Requirements: 13.2, 13.3, 13.6

RUN apt-get update && apt-get install -y --no-install-recommends \
    lsphp81-mysql lsphp81-curl lsphp81-intl \
    curl unzip wget bzip2 mariadb-client \
    && rm -rf /var/lib/apt/lists/*

RUN ln -sf /usr/local/lsws/lsphp81/bin/php /usr/local/bin/php 2>/dev/null || true

COPY build/ols_htaccess.so /usr/local/lsws/modules/ols_htaccess.so

RUN mkdir -p /var/www/vhosts/localhost/html/wordpress \
             /var/www/vhosts/localhost/html/nextcloud \
             /var/www/vhosts/localhost/html/drupal \
             /var/www/vhosts/localhost/html/laravel \
    && echo "<h1>App Stack OK</h1>" > /var/www/vhosts/localhost/html/index.html \
    && chown -R nobody:nogroup /var/www/vhosts/localhost

# Add ols_htaccess module to httpd_config.conf
RUN CONF="/usr/local/lsws/conf/httpd_config.conf" && \
    if ! grep -q 'ols_htaccess' "$CONF" 2>/dev/null; then \
      printf '\nmodule ols_htaccess {\n  ls_enabled              1\n}\n' >> "$CONF"; \
    fi

# Point the base image's lsphp fcgi-bin symlink to lsphp81 so our extensions work
RUN ln -sf /usr/local/lsws/lsphp81/bin/lsphp /usr/local/lsws/fcgi-bin/lsphp 2>/dev/null || true

# Debug: show final config in build log for CI troubleshooting
RUN echo "=== httpd_config.conf ===" && \
    cat /usr/local/lsws/conf/httpd_config.conf && \
    echo "" && echo "=== docker.conf template ===" && \
    cat /usr/local/lsws/conf/templates/docker.conf 2>/dev/null || true && \
    echo "" && echo "=== fcgi-bin ===" && \
    ls -la /usr/local/lsws/fcgi-bin/ 2>/dev/null || true

EXPOSE 80 443
