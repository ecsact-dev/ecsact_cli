# Changelog
All notable changes to this project will be documented in this file. See [conventional commits](https://www.conventionalcommits.org/) for commit guidelines.

- - -
## 0.3.8 - 2024-05-27
#### Features
- add debug flag (#102) - (8ce5d71) - Austin Kelway
#### Miscellaneous Chores
- ignore bazel lock files - (04e25f9) - Ezekiel Warren
- sync with ecsact_common (#81) - (e458c22) - seaubot

- - -

## 0.3.7 - 2024-05-17
#### Features
- always build with ECSACT_BUILD define (#98) - (dd252ff) - Ezekiel Warren

- - -

## 0.3.6 - 2024-05-10
#### Bug Fixes
- force clang to compile in c++ (#97) - (c0203cd) - Austin Kelway

- - -

## 0.3.5 - 2024-05-10
#### Bug Fixes
- error handling for codegen and recipe plugins work (#92) - (967aed2) - Austin Kelway
#### Features
- **(build)** merge multiple build recipes (#84) - (cc68d8d) - Ezekiel Warren
- support bundled codegen plugins (#90) - (9e666df) - Ezekiel Warren
- better error output when running with bazel (#91) - (df45dca) - Ezekiel Warren
- recipe bundle command (#85) - (b17ce9c) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update dependency bazel to v7.1.2 (#89) - (249211c) - renovate[bot]
- **(deps)** update dependency curl to v8.4.0.bcr.1 (#83) - (b99cc9d) - renovate[bot]

- - -

## 0.3.4 - 2024-04-04
#### Bug Fixes
- intermediate-dir for msvc cl (#80) - (f6dae10) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update crate-ci/typos action to v1.20.4 (#79) - (3fdaed4) - renovate[bot]

- - -

## 0.3.3 - 2024-04-04
#### Bug Fixes
- build recipe aware of async methods (#73) - (82aa0f1) - Ezekiel Warren
#### Features
- generate dylib preprocessor guard (#77) - (5330e18) - Ezekiel Warren
- report more msvc CL errors (#76) - (28d9027) - Ezekiel Warren
- allow c++ sources outside src dir (#75) - (431c80b) - Ezekiel Warren
- display C++ warnings/errors as messages (#74) - (9b8aed9) - Ezekiel Warren
#### Miscellaneous Chores
- sync with ecsact_common (#41) - (8ecf2f9) - seaubot
- update deps (#78) - (c1bdb39) - Ezekiel Warren

- - -

## 0.3.2 - 2024-03-28
#### Features
- **(build)** allow plugins relative to recipe file (#72) - (04ae855) - Austin Kelway

- - -

## 0.3.1 - 2024-03-27
#### Miscellaneous Chores
- update deps - (da9af3d) - Ezekiel Warren

- - -

## 0.3.0 - 2024-03-26
#### Bug Fixes
- multiple methods in recipe being detected (#52) - (82f908a) - Ezekiel Warren
- disable color text output on windows (#53) - (dd37ac4) - Ezekiel Warren
- add missing SDK includes (#49) - (ef84dac) - Ezekiel Warren
#### Features
- build receipe can now fetch sources (#71) - (d6d2b18) - Ezekiel Warren
- export dylib (#50) - (9aa13ab) - Ezekiel Warren
- coloured text output (#51) - (65523b8) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update jidicula/clang-format-action action to v4.11.0 (#5) - (ac6e02d) - renovate[bot]
- **(deps)** update escact repositories (#40) - (91ce484) - renovate[bot]
- **(deps)** update dependency platforms to v0.0.9 (#59) - (02b3a8a) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.5.0 (#60) - (de35022) - renovate[bot]
- **(deps)** update actions/cache action to v4 (#67) - (590b4f5) - renovate[bot]
- **(deps)** update dependency nlohmann_json to v3.11.3 (#63) - (3fce1be) - renovate[bot]
- **(deps)** update dependency rules_pkg to v0.10.1 (#66) - (a6a6d25) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to eca42c6 (#62) - (1567b15) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to ade23e0 (#61) - (c2258f4) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 7500623 (#58) - (c25491f) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 1e5f3c6 (#48) - (5f0f7d2) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to ac6411f (#47) - (66c43a4) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 0a9feb7 (#46) - (61f8c34) - renovate[bot]
- enable merge queue - (832cbd5) - Ezekiel Warren
- fix typos - (2cf8f73) - Ezekiel Warren
- remove rules_ecsact dependency (#68) - (7239e29) - Ezekiel Warren
- remove unneeded linux bzl flag - (4c3d26b) - Ezekiel Warren
- bzlmod updates - (8e52fe0) - Ezekiel Warren
- more methods in build recipe test (#54) - (51eb74a) - Ezekiel Warren

- - -

## 0.2.3 - 2023-10-06
#### Bug Fixes
- release was using wrong target/path - (8196a98) - Ezekiel Warren

- - -

## 0.2.2 - 2023-10-06
#### Features
- build subcommand (#13) - (7ec3f04) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update com_grail_bazel_toolchain digest to 42fa12b (#44) - (fe508e9) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to a76d197 (#43) - (88c77b7) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 91abcad (#42) - (b7c78aa) - renovate[bot]

- - -

## 0.2.1 - 2023-09-21
#### Features
- make --version flag optional build (#39) - (3a99019) - Ezekiel Warren
- allow ecsact cli used as a toolchain (#38) - (22d360b) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update com_grail_bazel_toolchain digest to 2733561 (#37) - (0fdab8d) - renovate[bot]

- - -

## 0.2.0 - 2023-09-21
#### Bug Fixes
- remove rules_cc_stamp dep (#35) - (c37aa71) - Ezekiel Warren
#### Features
- temporarily disable benchmark (#36) - (b8f702e) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update escact repositories (#28) - (9dcc061) - renovate[bot]
- **(deps)** update actions/checkout action to v4 (#25) - (38d796f) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 885e692 (#32) - (48403fc) - renovate[bot]
- sync with ecsact_common (#34) - (0113ec5) - seaubot
- sync with ecsact_common (#33) - (de5d466) - seaubot
- update ascii logo - (4f40e9e) - Ezekiel Warren

- - -

## 0.1.2 - 2023-09-19
#### Features
- bzlmodify (#30) - (3b0d8ea) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update com_grail_bazel_toolchain digest to 3e6d4d9 (#31) - (a906d3c) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to edc058a (#29) - (93a0d7c) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to e4fad4e (#27) - (8d272af) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to c217c03 (#26) - (de26be4) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 9d8cc8a (#24) - (c21d98c) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to e160627 (#22) - (6fe367c) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to ecafb96 (#21) - (196f413) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to aaa2fda (#20) - (0b4e271) - renovate[bot]
- **(deps)** update dependency rules_pkg to v0.9.1 (#16) - (30758ed) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.0.8 (#17) - (82a709e) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.4.2 (#15) - (bc8b24f) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to 86dbf52 (#19) - (1bb11a7) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to f94335f (#14) - (eed9fde) - renovate[bot]
- **(deps)** update dependency platforms to v0.0.7 (#18) - (fc65ae7) - renovate[bot]
- add module version - (4f57817) - Ezekiel Warren
- sync with ecsact_common (#23) - (5347bc1) - seaubot

- - -

## 0.1.1 - 2023-06-07
#### Miscellaneous Chores
- **(deps)** update escact repositories (#6) - (6852043) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to ceeedcc (#11) - (7af05d9) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to b397ad2 (#10) - (b5528b3) - renovate[bot]
- **(deps)** update com_grail_bazel_toolchain digest to 41ff2a0 (#9) - (c73a3b5) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to 3dddf20 (#8) - (4379847) - renovate[bot]
- **(deps)** update hedron_compile_commands digest to 80ac7ef (#7) - (563cfa8) - renovate[bot]

- - -

## 0.1.0 - 2023-05-01
#### Miscellaneous Chores
- add github actions (#1) - (9b82e95) - Ezekiel Warren

- - -

Changelog generated by [cocogitto](https://github.com/cocogitto/cocogitto).
