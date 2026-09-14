import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  // Workspace packages are plain TS sources; let Next compile them.
  transpilePackages: ['@sim/estimator', '@sim/protocol'],
  reactStrictMode: true,
};

export default nextConfig;
