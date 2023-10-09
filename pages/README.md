# rebecca-zavou.github.io
assignment 0


# Check out solution URL
$ cat solution.txt
rebecca-zavou.github.io
# Ensure the URL exists
$ curl --output /dev/null --silent --head --fail rebecca-zavou.github.io \
&& echo "URL exists" || echo "URL does not exist"
URL exists
