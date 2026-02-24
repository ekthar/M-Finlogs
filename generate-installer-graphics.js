const puppeteer = require('puppeteer');
const path = require('path');

async function generateInstallerGraphics() {
    console.log('Launching browser...');
    const browser = await puppeteer.launch({
        headless: 'new',
        args: ['--no-sandbox', '--disable-setuid-sandbox']
    });

    try {
        const page = await browser.newPage();

        // Generate sidebar image (164x314)
        console.log('Generating installer sidebar...');
        await page.setViewport({ width: 164, height: 314, deviceScaleFactor: 1 });
        await page.goto(`file://${path.join(__dirname, 'assets', 'installer-sidebar.html')}`, {
            waitUntil: 'networkidle0'
        });
        await page.screenshot({
            path: path.join(__dirname, 'assets', 'installerSidebar.png'),
            type: 'png',
            omitBackground: false
        });
        console.log('✓ Sidebar generated (164x314 PNG)');

        // Generate header image (150x57)
        console.log('Generating installer header...');
        await page.setViewport({ width: 150, height: 57, deviceScaleFactor: 1 });
        await page.goto(`file://${path.join(__dirname, 'assets', 'installer-header.html')}`, {
            waitUntil: 'networkidle0'
        });
        await page.screenshot({
            path: path.join(__dirname, 'assets', 'installerHeader.png'),
            type: 'png',
            omitBackground: false
        });
        console.log('✓ Header generated (150x57 PNG)');

    } finally {
        await browser.close();
    }

    console.log('\n✓ Installer graphics generated as PNG files!');
    console.log('Note: electron-builder will automatically convert to BMP if needed.');
}

generateInstallerGraphics().catch(console.error);
