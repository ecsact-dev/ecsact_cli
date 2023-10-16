let dir = $env.FILE_PWD;
let ecsact_file = $dir + '/test.ecsact';
let recipe_file = $dir + '/example-recipe.yml';
let output_file = $dir + '/tmp/test';
let temp_dir = $dir + '/tmp';

# bazel run @ecsact_cli -- build $ecsact_file -r $recipe_file -o $output_file --temp_dir $temp_dir -f text
ecsact build $ecsact_file -r $recipe_file -o $output_file --temp_dir $temp_dir -f text
