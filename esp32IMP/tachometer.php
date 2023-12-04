<!DOCTYPE html>
<html>
	<head>
		<meta charset="UTF-8" />
		<title>IMP 2023/24 - Tachometr</title>
		<style>
			.centered-div {
				width: 30%;
				height: full;
				margin: 0 auto;
			}
		</style>
	</head>

	<body>
		<div class="centered-div">
			<h1>IMP 2023/24 - Tachometr</h1>
			<h3>Timestamps:</h3>
			<?php
                $file = fopen("tachometer.txt", "r") or die("Unable to open file!");
                while(!feof($file)) {
                    echo fgets($file) . "<br>"; } fclose($file); ?>
		</div>
	</body>
</html>
