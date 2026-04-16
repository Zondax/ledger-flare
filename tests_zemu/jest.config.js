module.exports = {
  preset: 'ts-jest',
  testEnvironment: 'node',
  // @zondax/zemu 0.66+ pulls get-port@7 which ships as pure ESM. Route .js
  // files through ts-jest (which transpiles `import` to `require` using the
  // CommonJS tsconfig module setting) and allow get-port to be transformed
  // instead of being skipped as a node_modules entry.
  transform: {
    '^.+\\.[jt]sx?$': ['ts-jest', { isolatedModules: true }],
  },
  transformIgnorePatterns: ['/node_modules/(?!get-port/)'],
}
