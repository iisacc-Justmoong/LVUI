from pathlib import Path
import re,sys
root=Path(sys.argv[1]);host=(root/'src/Overlays/NativeAlertButton.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('Alert button directly depends on another SDK')
for token in ('AlertButtonBinding','AlertButtonModelPolicy','NativeAlertButtonCallbacksValue'):
    if token not in host:raise SystemExit('Independent alert button binding missing: '+token)
for token in ('class NativeAlertButton final','builder.retain(','builder.on_publish(','builder.on_retire('):
    if token in host:raise SystemExit('LVUI still owns alert button host: '+token)
if 'class AlertButton final' in (root/'src/Overlays/AlertButton.h').read_text():
    raise SystemExit('LVUI still owns alert button visual calculation')
alert=(root/'src/Overlays/NativeAlert.h').read_text()
if 'AlertBinding' not in alert or 'NativeAlertButtonModel' not in alert or 'Presentation::template action' not in alert:
    raise SystemExit('Alert does not inject the alert button model into its presentation')
modal=(root/'src/Overlays/NativeModal.h').read_text()
if 'ModalBinding' not in modal or 'NativeAlertButtonModel' not in modal or 'Presentation::template action' not in modal:
    raise SystemExit('Modal does not inject the alert button model into its presentation')
