const puppeteer = require('puppeteer');
const path = require('path');
const fs = require('fs');

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
        await page.setViewport({ width: 164, height: 314, deviceScaleFactor: 2 });
        await page.goto(`file://${path.join(__dirname, 'assets', 'installer-sidebar.html')}`, {
            waitUntil: 'networkidle0'
        });
        await page.screenshot({
            path: path.join(__dirname, 'assets', 'installerSidebar.bmp'),
            type: 'png', // Generate PNG first, will be converted to BMP
            omitBackground: false
        });
        console.log('✓ Sidebar generated');

        // Generate header image (150x57)
        console.log('Generating installer header...');
        await page.setViewport({ width: 150, height: 57, deviceScaleFactor: 2 });
        await page.goto(`file://${path.join(__dirname, 'assets', 'installer-header.html')}`, {
            waitUntil: 'networkidle0'
        });
        await page.screenshot({
            path: path.join(__dirname, 'assets', 'installerHeader.bmp'),
            type: 'png', // Generate PNG first, will be converted to BMP
            omitBackground: false
        });
        console.log('✓ Header generated');

    } finally {
        await browser.close();
    }

    console.log('\n✓ Installer graphics generated successfully!');
    console.log('Note: electron-builder can use PNG files. If BMP is required, convert using ImageMagick:');
    console.log('  magick convert assets/installerSidebar.bmp assets/installerSidebar.bmp');
}

generateInstallerGraphics().catch(console.error);
