# syntax=docker/dockerfile:1.4

FROM mcr.microsoft.com/dotnet-buildtools/prereqs:centos-7-f39df28-20191023143754 AS build

RUN yum install -y clang

WORKDIR /app
COPY . .
ENV CORECLR_PATH=/app/runtime

RUN --mount=from=runtime,target=/app/runtime ./build.sh