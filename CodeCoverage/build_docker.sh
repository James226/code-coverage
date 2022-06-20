#!/bin/sh
docker buildx build --build-context=runtime=https://github.com/dotnet/coreclr.git#v3.1.25 -t code-coverage:0.2 .
