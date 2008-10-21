<group>
<module name="std_ole_v2" type="embeddedtypelib">
	<include base="std_ole_v2">.</include>
	<include base="ReactOS">include/reactos/wine</include>
	<define name="__WINESRC__" />
	<define name="_WIN32_IE">0x600</define>
	<define name="_WIN32_WINNT">0x501</define>
	<define name="WINVER">0x501</define>
	<file>std_ole_v2.idl</file>
</module>
<module name="stdole2.tlb" type="win32dll" extension=".tlb" installbase="system32" installname="stdole2.tlb" entrypoint="0">
	<importlibrary definition="stdole2.tlb.spec" />
	<include base="stdole2.tlb">.</include>
	<include base="stdole2.tlb" root="intermediate">.</include>
	<include base="ReactOS">include/reactos/wine</include>
	<dependency>std_ole_v2</dependency>
	<define name="__WINESRC__" />
	<define name="_WIN32_IE">0x600</define>
	<define name="_WIN32_WINNT">0x501</define>
	<define name="WINVER">0x501</define>
	<library>kernel32</library>
	<file>stdole2.tlb.spec</file>
	<file>rsrc.rc</file>
</module>
</group>