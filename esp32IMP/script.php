<?php
$file = 'data.txt';
$data = file_get_contents('php://input');

file_put_contents($file, $data . PHP_EOL, FILE_APPEND);

echo "Data written to file.";
?>