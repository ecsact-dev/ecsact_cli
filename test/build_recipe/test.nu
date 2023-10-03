let dir = $env.FILE_PWD;
let ecsact_file = $dir + '/test.ecsact';
let recipe_file = $dir + '/ecsact_build_test_recipe.yml';
let output_file = $dir + '/tmp/test.dll';
let temp_dir = $dir + '/tmp';

bazel run @ecsact_cli -- build $ecsact_file -r $recipe_file -o $output_file --temp_dir $temp_dir -f json
