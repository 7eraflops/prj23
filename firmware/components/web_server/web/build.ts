/// <reference types="bun-types" />

async function build() {
  console.log("Building frontend with Bun...");

  // 1. Bundle and minify the TypeScript code
  const result = await Bun.build({
    entrypoints: ["./src/main.ts"],
    minify: true,
    target: "browser",
  });

  if (!result.success) {
    console.error("Build failed:");
    console.error(result.logs);
    process.exit(1);
  }

  // 2. Extract the compiled JavaScript
  const jsCode = await result.outputs[0].text();

  // 3. Read the source HTML file
  let html = await Bun.file("./src/index.html").text();

  // 4. Inline the JavaScript directly into the HTML
  html = html.replace(
    '<script type="module" src="/src/main.ts"></script>',
    `<script>\n${jsCode}\n</script>`,
  );

  // 5. Write the final single-file HTML to the component root directory
  await Bun.write("../index.html", html);

  console.log("Build complete! Embedded output saved to ../index.html");
}

build().catch(console.error);
