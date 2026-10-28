#!/system/bin/sh

resetprop -w init.svc.bootanim running

[ $(realpath "/proc/$$/exe") != "/system/bin/sh" ] && exec "/system/bin/sh" "$0"

echo $'#!/system/bin/sh\n[ $(exec /system/bin/id -u) -lt 10000 ] && exec /system/bin/cmd package "$@"\nexec /system/bin/cmd package "$@" | exec /system/bin/sed \'s/lineage/lineagx/g; s/Lineage/Lineagx/g\'' > "/data/system/pm"
chmod "$(stat -c "%a" "/system/bin/pm")" "/data/system/pm"
chown "$(stat -c "%u:%g" "/system/bin/pm")" "/data/system/pm"
chcon "$(stat -c "%C" "/system/bin/pm")" "/data/system/pm"
nsenter -m -t 1 -- mount --bind "/data/system/pm" "/system/bin/pm"

[ "$(getprop ro.product.model)" = "KB2000" ] && (
    if [ -f "/system/bin/service_" ] && grep -q 'id -u) -lt 10000' "/system/bin/service" 2>/dev/null; then exit; fi
    mount -o remount,rw /
    [ -f "/system/bin/service_" ] || cp -a "/system/bin/service" "/system/bin/service_"
    cat > "/system/bin/service" << 'EOF_'
#!/system/bin/sh
[ $(exec /system/bin/id -u) -lt 10000 ] && exec /system/bin/service_ "$@"
case "$@" in
    "list")
        exec /system/bin/cat << 'EOF'
Found 392 services:
0	AtlasService: [android.atlas.service]
1	DockObserver: []
2	ISubsysRadio: [com.oplus.telephony.ISubsysRadio]
3	IVirtualComm: [com.oplus.virtualcomm.IVirtualCommService]
4	MMListService: [IMMListService]
5	OPLUSExService: [android.os.IOplusExService]
6	OplusLocationManager: [android.location.IOplusLocationManager]
7	OplusShoulderPressureManagerService: [com.oplus.shoulderpressure.IOplusShoulderPressureManager]
8	SurfaceFlinger: [android.ui.ISurfaceComposer]
9	SurfaceFlingerAIDL: [android.gui.ISurfaceComposer]
10	accessibility: [android.view.accessibility.IAccessibilityManager]
11	account: [android.accounts.IAccountManager]
12	activity: [android.app.IActivityManager]
13	activity_task: [android.app.IActivityTaskManager]
14	adb: [android.debug.IAdbManager]
15	additionald: [android.os.IOplusService]
16	adservices_manager: [android.app.adservices.IAdServicesManager]
17	advanced_protection: [android.security.advancedprotection.IAdvancedProtectionService]
18	aiwidget: [com.oplus.aiwidget.IAIWidgetManagerService]
19	alarm: [android.app.IAlarmManager]
20	alipay: [android.hardware.alipay.IAlipayService]
21	android.frameworks.cameraservice.service.ICameraService/default: [android.frameworks.cameraservice.service.ICameraService]
22	android.frameworks.devicestate.IDeviceStateService/default: [android.frameworks.devicestate.IDeviceStateService]
23	android.frameworks.location.altitude.IAltitudeService/default: [android.frameworks.location.altitude.IAltitudeService]
24	android.frameworks.sensorservice.ISensorManager/default: [android.frameworks.sensorservice.ISensorManager]
25	android.frameworks.stats.IStats/default: [android.frameworks.stats.IStats]
26	android.frameworks.vibrator.IVibratorControlService/default: [android.frameworks.vibrator.IVibratorControlService]
27	android.hardware.biometrics.face.IFace/default: [android.hardware.biometrics.face.IFace]
28	android.hardware.light.ILights/default: [android.hardware.light.ILights]
29	android.hardware.media.c2.IComponentStore/software: [android.hardware.media.c2.IComponentStore]
30	android.hardware.power.IPower/default: [android.hardware.power.IPower]
31	android.hardware.vibrator.IVibrator/default: [android.hardware.vibrator.IVibrator]
32	android.media.audio.IHalAdapterVendorExtension/default: [android.media.audio.IHalAdapterVendorExtension]
33	android.os.UpdateEngineService: [android.os.IUpdateEngine]
34	android.os.UpdateEngineStableService: [android.os.IUpdateEngineStable]
35	android.security.apc: [android.security.apc.IProtectedConfirmation]
36	android.security.authorization: [android.security.authorization.IKeystoreAuthorization]
37	android.security.compat: [android.security.compat.IKeystoreCompatService]
38	android.security.identity: [android.security.identity.ICredentialStoreFactory]
39	android.security.legacykeystore: [android.security.legacykeystore.ILegacyKeystore]
40	android.security.maintenance: [android.security.maintenance.IKeystoreMaintenance]
41	android.security.metrics: [android.security.metrics.IKeystoreMetrics]
42	android.service.gatekeeper.IGateKeeperService: [android.service.gatekeeper.IGateKeeperService]
43	android.system.keystore2.IKeystoreService/default: [android.system.keystore2.IKeystoreService]
44	android.system.net.netd.INetd/default: [android.system.net.netd.INetd]
45	android.system.suspend.ISystemSuspend/default: [android.system.suspend.ISystemSuspend]
46	android.system.vmtethering.IVmTethering: [android.system.vmtethering.IVmTethering]
47	android.system.vold.IVold/default: [android.system.vold.IVold]
48	anti_root_dialog: [com.oplus.exsystemservice.antirootdialog.IAntiRootDialog]
49	app_binding: []
50	app_compatibility: [com.oplus.compatibility.IOplusCompatibilityService]
51	app_function: [android.app.appfunctions.IAppFunctionManager]
52	app_hibernation: [android.apphibernation.IAppHibernationService]
53	app_integrity: [android.content.integrity.IAppIntegrityManager]
54	app_search: [android.app.appsearch.aidl.IAppSearchManager]
55	appops: [com.android.internal.app.IAppOpsService]
56	appwidget: [com.android.internal.appwidget.IAppWidgetService]
57	athenaservice: [com.oplus.app.IAthenaService]
58	atomacq: [com.oplus.midas.IAtomsAcquire]
59	attestation_verification: [android.security.attestationverification.IAttestationVerificationManagerService]
60	audio: [android.media.IAudioService]
61	auth: [android.hardware.biometrics.IAuthService]
62	authentication_policy: [android.security.authenticationpolicy.IAuthenticationPolicyService]
63	autofill: [android.view.autofill.IAutoFillManager]
64	background_install_control: [android.content.pm.IBackgroundInstallControlService]
65	backup: [android.app.backup.IBackupManager]
66	battery: []
67	batteryproperties: [android.os.IBatteryPropertiesRegistrar]
68	batterystats: [com.android.internal.app.IBatteryStats]
69	binder_calls_stats: []
70	biometric: [android.hardware.biometrics.IBiometricService]
71	blob_store: [android.app.blob.IBlobStoreManager]
72	bluetooth_manager: [android.bluetooth.IBluetoothManager]
73	bugreport: [android.os.IDumpstate]
74	cabc: [com.android.internal.cabc.ICabcManager]
75	cacheinfo: []
76	carrier_config: [com.android.internal.telephony.ICarrierConfigLoader]
77	clipboard: [android.content.IClipboard]
78	cmcc5gslice: [android.cmccslice.ICmccSliceManager]
79	color_display: [android.hardware.display.IColorDisplayManager]
80	color_screenshot: [com.oplus.screenshot.IOplusScreenshotManager]
81	commscene: [com.oplus.commscene.ICommScene]
82	companiondevice: [android.companion.ICompanionDeviceManager]
83	connectivity: [android.net.IConnectivityManager]
84	connectivity_native: [android.net.connectivity.aidl.ConnectivityNative]
85	connmetrics: [android.net.IIpConnectivityMetrics]
86	content: [android.content.IContentService]
87	country_detector: [android.location.ICountryDetector]
88	cpuinfo: []
89	credential: [android.credentials.ICredentialManager]
90	critical.log: [BBinder]
91	cross_device_service: []
92	crossprofileapps: [android.content.pm.ICrossProfileApps]
93	dataloader_manager: [android.content.pm.IDataLoaderManager]
94	dbinfo: []
95	device_config: []
96	device_identifiers: [android.os.IDeviceIdentifiersPolicyService]
97	device_lock: [android.devicelock.IDeviceLockService]
98	device_policy: [android.app.admin.IDevicePolicyManager]
99	device_state: [android.hardware.devicestate.IDeviceStateManager]
100	deviceidle: [android.os.IDeviceIdleController]
101	devicestoragemonitor: []
102	diskstats: []
103	display: [android.hardware.display.IDisplayManager]
104	display.smomoservice: [android.display.ISmomoService]
105	dnsresolver: [android.net.IDnsResolver]
106	domain_verification: [android.content.pm.verify.domain.IDomainVerificationManager]
107	dpmservice: [com.qti.dpm.IDpmService]
108	dreams: [android.service.dreams.IDreamManager]
109	drm.drmManager: [drm.IDrmManagerService]
110	dropbox: [com.android.internal.os.IDropBoxManagerService]
111	dynamic_filter: [com.oplus.filter.IDynamicFilterService]
112	dynamic_instrumentation: [android.os.instrumentation.IDynamicInstrumentationManager]
113	dynamic_system: [android.os.image.IDynamicSystemService]
114	ecm_enhanced_confirmation: [android.app.ecm.IEnhancedConfirmationManager]
115	emergency_affordance: []
116	engineer: [android.engineer.IOplusEngineerManager]
117	engineer_fw: [com.android.server.engineering.IOplusEngineerFwService]
118	ethernet: [android.net.IEthernetManager]
119	external_vibrator_service: [android.os.IExternalVibratorService]
120	face: [android.hardware.face.IFaceService]
121	feature_flags: [android.flags.IFeatureFlags]
122	file_integrity: [android.security.IFileIntegrityService]
123	fingerprint: [android.hardware.fingerprint.IFingerprintService]
124	font: [com.android.internal.graphics.fonts.IFontManager]
125	gaiad: [android.gaia.IOplusGaia]
126	game: [android.app.IGameManagerService]
127	gfxinfo: []
128	gpu: [android.graphicsenv.IGpuService]
129	grammatical_inflection: [android.app.IGrammaticalInflectionManager]
130	graphicsstats: [android.view.IGraphicsStats]
131	guardelfthermalcontrol: [android.os.IGuardElfThermalControl]
132	hardware_properties: [android.os.IHardwarePropertiesManager]
133	healthconnect: [android.health.connect.aidl.IHealthConnectService]
134	heimdall: [com.oplus.heimdall.IHeimdallService]
135	horae: [com.oplus.horae.IHoraeService]
136	imms: [com.android.internal.telephony.IMms]
137	incident: [android.os.IIncidentManager]
138	incidentcompanion: [android.os.IIncidentCompanion]
139	incremental: [android.os.incremental.IIncrementalService]
140	input: [android.hardware.input.IInputManager]
141	input_method: [com.android.internal.view.IInputMethodManager]
142	inputflinger: [android.os.IInputFlinger]
143	installd: [android.os.IInstalld]
144	intrusion_detection: [android.security.intrusiondetection.IIntrusionDetectionService]
145	ions: [com.android.internal.telephony.IOns]
146	iopgp.service: [iopgp.service]
147	iphonesubinfo: [com.android.internal.telephony.IPhoneSubInfo]
148	ipsec: [android.net.IIpSecService]
149	isms: [com.android.internal.telephony.ISms]
150	isms_ext: [com.oplus.telephony.ISmsExt]
151	isub: [com.android.internal.telephony.ISub]
152	isub_ext: [com.oplus.telephony.ISubExt]
153	jobscheduler: [android.app.job.IJobScheduler]
154	launcherapps: [android.content.pm.ILauncherApps]
155	legacy_permission: [android.permission.ILegacyPermissionManager]
156	lights: [android.hardware.lights.ILightsManager]
157	linearmotor: [com.oplus.os.ILinearmotorVibratorService]
158	locale: [android.app.ILocaleManager]
159	location: [android.location.ILocationManager]
160	location_time_zone_manager: []
161	lock_settings: [com.android.internal.widget.ILockSettings]
162	logcat: [android.os.logcat.ILogcatManagerService]
163	looper_stats: []
164	manager: [android.os.IServiceManager]
165	mdns: [android.net.mdns.aidl.IMDns]
166	media.aaudio: [aaudio.IAAudioService]
167	media.audio_flinger: [android.media.IAudioFlingerService]
168	media.audio_policy: [android.media.IAudioPolicyService]
169	media.camera: [android.hardware.ICameraService]
170	media.camera.proxy: [android.hardware.ICameraServiceProxy]
171	media.extractor: [android.IMediaExtractorService]
172	media.metrics: [android.media.IMediaMetricsService]
173	media.player: [android.media.IMediaPlayerService]
174	media.resource_manager: [android.media.IResourceManagerService]
175	media.resource_observer: [android.media.IResourceObserverService]
176	media_communication: [android.media.IMediaCommunicationService]
177	media_metrics: [android.media.metrics.IMediaMetricsManager]
178	media_projection: [android.media.projection.IMediaProjectionManager]
179	media_resource_monitor: [android.media.IMediaResourceMonitor]
180	media_router: [android.media.IMediaRouterService]
181	media_session: [android.media.session.ISessionManager]
182	meminfo: []
183	memtrack.proxy: [android.hardware.memtrack.IMemtrack]
184	midasd: [com.oplus.otrace.IMidasService]
185	midi: [android.media.midi.IMidiManager]
186	mount: [android.os.storage.IStorageManager]
187	multimediaDaemon: [com.oplus.atlas.IOplusMMAtlasService]
188	nearby: [android.nearby.INearbyManager]
189	netd: [android.net.INetd]
190	netd_listener: [android.net.metrics.INetdEventListener]
191	netpolicy: [android.net.INetworkPolicyManager]
192	netstats: [android.net.INetworkStatsService]
193	network_management: [android.os.INetworkManagementService]
194	network_score: [android.net.INetworkScoreService]
195	network_stack: [android.net.INetworkStackConnector]
196	network_time_update_service: []
197	network_watchlist: [com.android.internal.net.INetworkWatchlistManager]
198	networking_control: [android.net.IOplusNetworkingControlManager]
199	neuronsystem: [com.oplus.neuron.INeuronSystemService]
200	nfc: [android.nfc.INfcAdapter]
201	nfcextnsservice: [com.oplus.nfcextns.INfcExtnsService]
202	notification: [android.app.INotificationManager]
203	nwdiagnose: [com.android.internal.telephony.INetworkDiagnoseService]
204	oappnetcontrol: [android.nwpower.IOAppNetControl]
205	obrain: [com.oplus.midas.IOplusObrain]
206	ocenter: [com.oplus.ocenter.api.IOCenter]
207	oem_lock: [android.service.oemlock.IOemLockService]
208	oemlinklatency: [com.android.internal.telephony.IOemLinkLatencyManager]
209	oiface: [com.oppo.oiface.IOIfaceService]
210	olc: [android.os.olc.IOlcService]
211	on_device_intelligence: [android.app.ondeviceintelligence.IOnDeviceIntelligenceManager]
212	ondevicepersonalization_system_service: [android.ondevicepersonalization.IOnDevicePersonalizationSystemService]
213	onetrace: [android.os.IOplusTraceService]
214	operator: [android.operator.IOplusOperatorManager]
215	oplus.hans.IHansComunication: [oplus.hans.IHansComunication]
216	oplus_accesscontrol: [com.oplus.app.IOplusAccessControlManager]
217	oplus_app_cache_service: [com.oplus.appcache.IOplusAppCacheService]
218	oplus_app_data_service: [com.oplus.exsystemservice.appdata.IOplusAppDataService]
219	oplus_app_stability_service: [com.oplus.stability.appstability.IOplusAppStabilityManager]
220	oplus_battery_stats_service: [com.android.internal.os.IOplusBatteryStatsService]
221	oplus_freeze: [com.oplus.app.IOplusHansFreezeManager]
222	oplus_mdlog: [com.oplus.log.IMdLogService]
223	oplus_media_control: [com.oplus.media.IOplusMediaControlManager]
224	oplus_nec: [com.oplus.nec.IOplusNecService]
225	oplus_perf_service: [com.oplus.performance.IOplusPerfService]
226	oplus_sandbox: [com.oplus.sandbox.IOplusSandboxManager]
227	oplus_sensor_service: [com.oplus.hardware.sensor.IOplusSensorService]
228	oplus_systemanropt_service: [android.app.ISystemAnrOptService]
229	oplus_telephony_ext: [com.android.internal.telephony.IOplusTelephonyExt]
230	oplus_telephony_internal_ext: [com.android.internal.telephony.IOplusTelephonyInternalExt]
231	oplus_tile: [com.oplus.quicksettings.IOplusTileManagerService]
232	oplus_vrr_service: [com.oplus.vrr.IOPlusRefreshRate]
233	opluscustomize: [android.os.customize.IOplusCustomizeService]
234	oplusdatalimit: [com.oplus.network.IOplusDataLimit]
235	oplusdevicepolicy: [android.os.oplusdevicepolicy.IOplusDevicePolicyManagerService]
236	oplusdynamicvsync: [com.oplus.dynamicvsync.IOplusDynamicVsync]
237	opluslinkkit: [com.oplus.network.IOlk]
238	oplusnaspower: [com.oplus.nas.power.INasPower]
239	oplusnetcmd: [com.oplus.network.IOplusNetworkCmdService]
240	oplusnetd: [android.net.IOplusNetd]
241	oplusnetworkengineer: [com.oplus.network.IOplusNetworkTcForEngineer]
242	oplusnetworkmanagement: [com.oplus.network.IOplusNetworkManagementService]
243	oplusnetworkstack: [com.oplus.network.IOplusNetworkStack]
244	oplusnetworkstats: [com.oplus.network.IOplusNetworkStatsService]
245	oplusoiface: [com.oplus.oiface.IOIfaceService]
246	opluspreodex: [com.oplus.os.IOplusPreOdex]
247	oplusscreenmode: [com.oplus.screenmode.IOplusScreenMode]
248	oplusstoragemanagerservice: [android.os.storage.IOplusStorageManagerService]
249	opluswifikitservice: [android.net.wifi.IOplusWifiManager]
250	osensemanager: [com.oplus.osense.IOsenseResManager]
251	ostats_pullerd: [com.oplus.ostats.IOstatsPullerService]
252	ostats_tpd: [com.oplus.ostats.IOStatsTpdService]
253	ostatsd: [com.oplus.ostats.api.IOStats]
254	overlay: [android.content.om.IOverlayManager]
255	pac_proxy: [android.net.IPacProxyManager]
256	package: [android.content.pm.IPackageManager]
257	package_native: [android.content.pm.IPackageManagerNative]
258	people: [android.app.people.IPeopleManager]
259	performance_hint: [android.os.IHintManager]
260	permission: [android.os.IPermissionController]
261	permission_checker: [android.permission.IPermissionChecker]
262	permissionmgr: [android.permission.IPermissionManager]
263	persistent_data_block: [android.service.persistentdata.IPersistentDataBlockService]
264	phone: [com.android.internal.telephony.ITelephony]
265	pinner: [android.app.pinner.IPinnerService]
266	platform_compat: [com.android.internal.compat.IPlatformCompat]
267	platform_compat_native: [com.android.internal.compat.IPlatformCompatNative]
268	plmn_carrier_config: [com.android.internal.telephony.IPlmnCarrierConfigLoader]
269	power: [android.os.IPowerManager]
270	power_monitor: [com.oplus.os.IOplusPowerMonitor]
271	powerstats: [android.os.IPowerStatsService]
272	print: [android.print.IPrintManager]
273	processinfo: [android.os.IProcessInfoService]
274	procstats: [com.android.internal.app.procstats.IProcessStats]
275	profiling_service: [android.os.IProfilingService]
276	ranging: [android.ranging.IRangingAdapter]
277	reboot_readiness: [android.scheduling.IRebootReadinessManager]
278	recovery: [android.os.IRecoverySystem]
279	remote_provisioning: [android.security.rkp.IRemoteProvisioning]
280	resources: [android.content.res.IResourcesManager]
281	restrictions: [android.content.IRestrictionsManager]
282	role: [android.app.role.IRoleManager]
283	rollback: [android.content.rollback.IRollbackManager]
284	runtime: []
285	safety_center: [android.safetycenter.ISafetyCenterManager]
286	scheduling_policy: [android.os.ISchedulingPolicyService]
287	sdk_sandbox: [android.app.sdksandbox.ISdkSandboxManager]
288	search: [android.app.ISearchManager]
289	sec_key_att_app_id_provider: [android.security.keystore.IKeyAttestationApplicationIdProvider]
290	secrecy: [android.secrecy.ISecrecyService]
291	secure_element: [android.se.omapi.ISecureElementService]
292	security_permission: [android.os.ISecurityPermissionService]
293	security_state: [android.os.ISecurityStateManager]
294	sensitive_content_protection_service: [android.view.ISensitiveContentProtectionManager]
295	sensor_privacy: [android.hardware.ISensorPrivacyManager]
296	sensorservice: [android.gui.SensorServer]
297	serial: [android.hardware.ISerialManager]
298	servicediscovery: [android.net.nsd.INsdManager]
299	settings: []
300	shortcut: [android.content.pm.IShortcutService]
301	simphonebook: [com.android.internal.telephony.IIccPhoneBook]
302	slice: [android.app.slice.ISliceManager]
303	soundtrigger: [com.android.internal.app.ISoundTriggerService]
304	soundtrigger_middleware: [android.media.soundtrigger_middleware.ISoundTriggerMiddlewareService]
305	speech_recognition: [android.speech.IRecognitionServiceManager]
306	stamp_service: [com.oplus.exsystemservice.postman.IOplusStampService]
307	stats: [android.os.IStatsd]
308	statsbootstrap: [android.os.IStatsBootstrapAtomService]
309	statscompanion: [android.os.IStatsCompanionService]
310	statsmanager: [android.os.IStatsManagerService]
311	statusbar: [com.android.internal.statusbar.IStatusBarService]
312	storage_healthinfo: [android.os.storage.IStorageHealthInfoService]
313	storagecompressdedup: [com.oplus.os.IAppCompressAndFileDedup]
314	storaged: [android.os.IStoraged]
315	storaged_pri: [android.os.storaged.IStoragedPrivate]
316	storagestats: [android.app.usage.IStorageStatsManager]
317	supervision: [android.app.supervision.ISupervisionManager]
318	suspend_control: [android.system.suspend.ISuspendControlService]
319	suspend_control_internal: [android.system.suspend.internal.ISuspendControlServiceInternal]
320	system_config: [android.os.ISystemConfig]
321	system_server_dumper: []
322	system_update: [android.os.ISystemUpdateManager]
323	telecom: [com.android.internal.telecom.ITelecomService]
324	telephony.registry: [com.android.internal.telephony.ITelephonyRegistry]
325	telephony_ims: [android.telephony.ims.aidl.IImsRcsController]
326	telephony_registry_ext: [com.android.internal.telephony.ITelephonyRegistryExt]
327	testharness: []
328	tethering: [android.net.ITetheringConnector]
329	textclassification: [android.service.textclassifier.ITextClassifierService]
330	textservices: [com.android.internal.textservice.ITextServicesManager]
331	texttospeech: [android.speech.tts.ITextToSpeechManager]
332	theia: [android.os.ITheiaManager]
333	thermalservice: [android.os.IThermalService]
334	time_detector: [android.app.timedetector.ITimeDetectorService]
335	time_zone_detector: [android.app.timezonedetector.ITimeZoneDetectorService]
336	tracing.proxy: [android.tracing.ITracingServiceProxy]
337	tradeinmode: [android.os.ITradeInMode]
338	transparency: [com.android.internal.os.IBinaryTransparencyService]
339	trust: [android.app.trust.ITrustManager]
340	tsa_bpf_service: [com.oplus.tsabpf.IOplusTsaManager]
341	uimode: [android.app.IUiModeManager]
342	updatelock: [android.os.IUpdateLock]
343	uri_grants: [android.app.IUriGrantsManager]
344	usage: [android.os.IOplusUsageService]
345	usagestats: [android.app.usage.IUsageStatsManager]
346	usb: [android.hardware.usb.IUsbManager]
347	user: [android.os.IUserManager]
348	vcn_management: [android.net.vcn.IVcnManagementService]
349	vendor.oplus.hardware.biometrics.fingerprintpay.IFingerprintPay/default: [vendor.oplus.hardware.biometrics.fingerprintpay.IFingerprintPay]
350	vendor.oplus.hardware.cammidasservice.IMIDASService/default: [vendor.oplus.hardware.cammidasservice.IMIDASService]
351	vendor.oplus.hardware.charger.ICharger/default: [vendor.oplus.hardware.charger.ICharger]
352	vendor.oplus.hardware.commondcs.ICommonDcsAidlHalService/default: [vendor.oplus.hardware.commondcs.ICommonDcsAidlHalService]
353	vendor.oplus.hardware.cryptoeng.ICryptoeng/default: [vendor.oplus.hardware.cryptoeng.ICryptoeng]
354	vendor.oplus.hardware.engineer.IEngineer/default: [vendor.oplus.hardware.engineer.IEngineer]
355	vendor.oplus.hardware.gameopt.IGameOptHalService/default: [vendor.oplus.hardware.gameopt.IGameOptHalService]
356	vendor.oplus.hardware.handlefactory.IHandleFactory/default: [vendor.oplus.hardware.handlefactory.IHandleFactory]
357	vendor.oplus.hardware.misc.IOplusMisc/default: [vendor.oplus.hardware.misc.IOplusMisc]
358	vendor.oplus.hardware.nfc.IOplusNfc/default: [vendor.oplus.hardware.nfc.IOplusNfc]
359	vendor.oplus.hardware.nfcExtns.INfcExtns/default: [vendor.oplus.hardware.nfcExtns.INfcExtns]
360	vendor.oplus.hardware.olc2.IOplusLogCore/default: [vendor.oplus.hardware.olc2.IOplusLogCore]
361	vendor.oplus.hardware.ormsHalService.IOrmsAidlHalService/default: [vendor.oplus.hardware.ormsHalService.IOrmsAidlHalService]
362	vendor.oplus.hardware.performance.IPerformance/default: [vendor.oplus.hardware.performance.IPerformance]
363	vendor.oplus.hardware.power.powermonitor.IPowerMonitor/default: [vendor.oplus.hardware.power.powermonitor.IPowerMonitor]
364	vendor.oplus.hardware.radio.IRadioStable/OplusRadio0: [vendor.oplus.hardware.radio.IOplusRadio]
365	vendor.oplus.hardware.radio.IRadioStable/OplusRadio1: [vendor.oplus.hardware.radio.IOplusRadio]
366	vendor.oplus.hardware.rpmh.IRpmh/default: [vendor.oplus.hardware.rpmh.IRpmh]
367	vendor.oplus.hardware.stability.oplus_project.IOplusProject/default: [vendor.oplus.hardware.stability.oplus_project.IOplusProject]
368	vendor.oplus.hardware.subsys_interface.subsys.ISubsys/default: [vendor.oplus.hardware.subsys_interface.subsys.ISubsys]
369	vendor.oplus.hardware.subsys_interface.subsys_radio.ISubsysRadio/slot1: [vendor.oplus.hardware.subsys_interface.subsys_radio.ISubsysRadio]
370	vendor.oplus.hardware.subsys_interface.subsys_radio.ISubsysRadio/slot2: [vendor.oplus.hardware.subsys_interface.subsys_radio.ISubsysRadio]
371	vendor.oplus.hardware.touch.IOplusTouch/default: [vendor.oplus.hardware.touch.IOplusTouch]
372	vendor.oplus.hardware.urcc.IUrcc/default: [vendor.oplus.hardware.urcc.IUrcc]
373	vendor.oplus.hardware.wifi.IOplusWifiService/default: [vendor.oplus.hardware.wifi.IOplusWifiService]
374	vendor.perfservice: [com.qualcomm.qti.IPerfManager]
375	vendor.qspmsvc: [Qspmsvc]
376	vendor.qti.hardware.radio.atcmdfwd.IAtCmdFwd/AtCmdFwdAidl: [vendor.qti.hardware.radio.atcmdfwd.IAtCmdFwd]
377	vendor.qti.qesdsys.IQesdSys/default: [vendor.qti.qesdsys.IQesdSys]
378	vibrator_manager: [android.os.IVibratorManagerService]
379	virtualdevice: [android.companion.virtual.IVirtualDeviceManager]
380	virtualdevice_native: [android.companion.virtualnative.IVirtualDeviceManagerNative]
381	voiceinteraction: [com.android.internal.app.IVoiceInteractionManagerService]
382	vold: [android.os.IVold]
383	vpn_management: [android.net.IVpnManager]
384	wallpaper: [android.app.IWallpaperManager]
385	wearable_sensing: [android.app.wearable.IWearableSensingManager]
386	webviewupdate: [android.webkit.IWebViewUpdateService]
387	wifi: [android.net.wifi.IWifiManager]
388	wifinl80211: [android.net.wifi.nl80211.IWificond]
389	wifip2p: [android.net.wifi.p2p.IWifiP2pManager]
390	wifiscanner: [android.net.wifi.IWifiScanner]
391	window: [android.view.IWindowManager]
EOF
    ;;
    *)
        exec "/system/bin/service_" "$@" | exec /system/bin/sed 's/lineage/lineagx/g; s/Lineage/Lineagx/g'
    ;;
esac
EOF_
    mount -o remount,ro "/"
) || {
    cat "/system/bin/service" > "/data/system/service_"
    echo $'#!/system/bin/sh\n[ $(exec /system/bin/id -u) -lt 10000 ] && exec "/data/system/service_" "$@"\nexec "/data/system/service_" "$@" | exec /system/bin/sed \'s/lineage/lineagx/g; s/Lineage/Lineagx/g\'' > "/data/system/service"
    chmod "$(stat -c "%a" "/system/bin/service")" "/data/system/service" "/data/system/service_"
    chown "$(stat -c "%u:%g" "/system/bin/service")" "/data/system/service" "/data/system/service_"
    chcon "$(stat -c "%C" "/system/bin/service")" "/data/system/service" "/data/system/service_"
    nsenter -m -t 1 -- mount --bind "/data/system/service" "/system/bin/service"
}

for i in "/system/build.prop" "/product/etc/build.prop" "/system_ext/etc/build.prop" "/system/system_dlkm/etc/build.prop"
do
    mp=$(df "$i" | awk 'NR==2 {print $6}')
    mount -o remount,rw "$mp"
    sed -i 's/userdebug/user/g' "$i"
    mount -o remount,ro "$mp"
done

resetprop -w sys.boot_completed 0

pm disable org.lineageos.jelly

pm list packages -s | sed 's/^package://' | sort -u > "${0%/*}/system_packages.txt"

until dumpsys window policy | awk '/showing=false/ { a=1 }; /inputRestricted=false/ { b=1 }; END { exit !(a && b) }'
do
    sleep 1
done

until [ -d "/sdcard/Android" ]
do
    sleep 1
done

resetprop -n "ro.boot.flash.locked" "1"
check_reset_prop() {
    local NAME=$1
    local EXPECTED=$2
    local VALUE=$(resetprop $NAME)
    [ -z $VALUE ] || [ $VALUE = $EXPECTED ] || resetprop -n $NAME $EXPECTED
}
contains_reset_prop() {
    local NAME=$1
    local CONTAINS=$2
    local NEWVAL=$3
    [[ "$(resetprop $NAME)" = *"$CONTAINS"* ]] && resetprop -n $NAME $NEWVAL
}
check_reset_prop "ro.boot.verifiedbootstate" "green"
check_reset_prop "ro.debuggable" "0"
check_reset_prop "ro.force.debuggable" "0"
check_reset_prop "ro.secure" "1"
check_reset_prop "ro.adb.secure" "1"
check_reset_prop "ro.build.type" "user"
check_reset_prop "ro.build.tags" "release-keys"
contains_reset_prop "ro.bootmode" "recovery" "unknown"

resetprop -n ro.boot.vbmeta.device_state "locked"
resetprop -n ro.boot.vbmeta.avb_version "1.4"
resetprop -n ro.boot.vbmeta.hash_alg "sha256"
resetprop -n ro.boot.vbmeta.size "4096"

resetprop -n init.svc.adb_root ""

resetprop -n ro.config.ringtone "unknown"

resetprop | awk -F '\\[|\\]: \\[|\\]' '/lineage/ {
    key=$2
    value=$3
    if (key ~ /lineage/) {
        system("exec resetprop --delete \"" key "\"")
    } else if (value ~ /lineage/) {
        gsub("lineage_?", "", value)
        system("exec resetprop \"" key "\" \"" value "\"")
    }
}'

[ -e /sys/module/susfs_guard_lkm ] && ksu_susfs() {
    case "$1" in
        add_sus_path*)
            echo "add $2" > /proc/susfs_path
        ;;
        add_sus_map)
            echo "add $2" > /proc/susfs_map
        ;;
        *)
            command ksu_susfs "$@"
        ;;
    esac
}

ksu_susfs hide_sus_mnts_for_non_su_procs 1
ksu_susfs enable_avc_log_spoofing 1

find "/data/resource-cache" "/system" "/vendor" "/system_ext" "/product" -iname "*lineage*" -o -iname '*gapps*' | while IFS= read -r line
do
    ksu_susfs add_sus_path "$line"
    [ -f "$line" ] && ksu_susfs add_sus_map "$line"
done

ksu_susfs add_sus_path "/system/addon.d"
ksu_susfs add_sus_path "/data/lineageos_updates"
ksu_susfs add_sus_path_loop "/system/lib64/libstagefright.so"
ksu_susfs add_sus_path "/data/adbroot"

ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_file_contexts"
ksu_susfs add_sus_path_loop "/system_ext/etc/selinux/system_ext_sepolicy.cil"
ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_sepolicy.cil"
ksu_susfs add_sus_map "/data/adb/modules/zygisksu/lib64/libzygisk.so"
ksu_susfs add_sus_map "/data/adb/modules/lineage_hide/zygisk/arm64-v8a.so"

sleep 30
ksu_susfs add_sus_path_loop "/vendor/etc/selinux/vendor_hwservice_contexts"
ksu_susfs add_sus_path_loop "/system_ext/etc/init/init.lineage-system_ext.rc"
ksu_susfs add_sus_path "/system_ext/etc/permissions"
