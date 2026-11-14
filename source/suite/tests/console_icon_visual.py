"""Render the bundled offline console with installed Windows Edge."""
from pathlib import Path
import argparse,base64,hashlib,importlib.util,json,os
from playwright.sync_api import sync_playwright
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[3];spec=importlib.util.spec_from_file_location('bundle',root/'source/tools/bundle_console.py');bundle=importlib.util.module_from_spec(spec);spec.loader.exec_module(bundle)
html=a.out/'console.html';html.write_text(bundle.bundle(root/'source/suite/frontend'),encoding='utf-8');source=(root/'source/suite/source/app-icon.png').read_bytes()
with sync_playwright() as pw:
 browser=pw.chromium.launch(channel='msedge',headless=True)
 for width in (1220,720):
  page=browser.new_page(viewport=dict(width=width,height=900));page.goto(html.as_uri());img=page.locator('.brand-mark img');page.wait_for_function('document.querySelector(".brand-mark img").complete');assert img.evaluate('(i)=>i.naturalWidth===128&&i.naturalHeight===128')
  uri=img.get_attribute('src');assert uri.startswith('data:image/png;base64,') and base64.b64decode(uri.split(',',1)[1])==source
  box=img.bounding_box();assert box['width']>=30 and box['height']>=30 and box['x']>=0
  assert img.evaluate('(i)=>getComputedStyle(i).imageRendering')=='pixelated'
  page.locator('.sidebar').screenshot(path=str(a.out/f'sidebar-{width}.png'));page.close()
 browser.close()
(a.out/'result.json').write_text(json.dumps(dict(passed=True,browser='installed Windows Edge',viewports=[1220,720],canonical_icon_sha256=hashlib.sha256(source).hexdigest(),game_tested=False),indent=2)+'\n');print('PASS embedded canonical PNG loaded and rendered at desktop/narrow widths in Windows Edge')
