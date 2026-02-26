FROM litespeedtech/openlitespeed:latest

# Copy the compiled ols_htaccess module
COPY build/ols_htaccess.so /usr/local/lsws/modules/ols_htaccess.so

# Copy OLS configuration files
COPY tests/e2e/conf/httpd_config.conf /usr/local/lsws/conf/httpd_config.conf
COPY tests/e2e/conf/vhconf.conf /usr/local/lsws/conf/vhosts/test/vhconf.conf

# Create test document root with static files for testing
RUN mkdir -p /var/www/vhosts/test/html/subdir && \
    echo "<h1>OK</h1>" > /var/www/vhosts/test/html/index.html && \
    echo "img" > /var/www/vhosts/test/html/photo.jpg && \
    echo "custom" > /var/www/vhosts/test/html/subdir/custom.html && \
    chown -R nobody:nogroup /var/www/vhosts/test

EXPOSE 8088
