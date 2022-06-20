ARG SDK_VERSION=3.1-buster
ARG RUNTIME_VERSION=3.1-buster-slim

#
# Install dependencies
#
FROM mcr.microsoft.com/dotnet/sdk:${SDK_VERSION} AS deps
WORKDIR /src

COPY CodeCoverage.Example.csproj CodeCoverage.Example.csproj

RUN dotnet restore CodeCoverage.Example.csproj

#
# Pull in source code
#
FROM deps AS src
WORKDIR /src

COPY . .

#
# Publish/build API
#
FROM src AS publish
WORKDIR /src

RUN dotnet build -c Release CodeCoverage.Example.csproj
RUN dotnet publish -c Release -o /publish --no-build CodeCoverage.Example.csproj

#
# Install OS dependencies
#
FROM mcr.microsoft.com/dotnet/core/aspnet:${RUNTIME_VERSION} AS base

#
# Run the application
#
FROM base AS app

USER 33 

WORKDIR /app
COPY --from=publish /publish .

ENV ASPNETCORE_URLS=http://+:8080
ENV ASPNETCORE_FORWARDEDHEADERS_ENABLED=true

ENTRYPOINT ["dotnet", "CodeCoverage.Example.dll"]

FROM app AS coverage

COPY --from=code-coverage:0.2 /app/CorProfiler.so .

ENV CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
ENV CORECLR_ENABLE_PROFILING=1
ENV CORECLR_PROFILER_PATH=/app/CorProfiler.so

FROM app AS final