/**
 * @type {import('@react-native-community/cli-types').UserDependencyConfig}
 */
module.exports = {
  dependency: {
    platforms: {
      android: {
        cxxModuleCMakeListsModuleName: 'react-native-mean-n-super-resolution',
        cxxModuleCMakeListsPath: 'CMakeLists.txt',
        cxxModuleHeaderName: 'MeanNSuperResolutionImpl',
      },
    },
  },
};
