# MQTT findings - Anycubic_1.1.27_apkcombo.com

Source analysed: ~/apk/Anycubic_1.1.27_apkcombo.com
Generated: 2026-03-12 22:51:39 UTC

## 1) MQTT endpoints / topics (resources)
897:    <string name="dev_mqtt_host">storage.cloud.anycubic.com</string>
1834:    <string name="mqtt_server_port">8883</string>
2250:    <string name="release_mqtt_host">mqtt.anycubicloud.com</string>
2543:    <string name="test_mqtt_host">mqtt-test.anycubicloud.com</string>
2570:    <string name="topic_plus">anycubic/anycubicCloud/v1/+/public/</string>
2571:    <string name="topic_printer">anycubic/anycubicCloud/v1/printer/app/</string>
2572:    <string name="topic_publish">anycubic/anycubicCloud/v1/app/</string>
2573:    <string name="topic_server">anycubic/anycubicCloud/v1/server/app/</string>

## 2) Android manifest + service registration
244:        <service android:name="com.cloud.mqttservice.MqttService"/>

## 3) Runtime init of MQTT constants from resources
    new-array v3, v2, [Ljava/lang/Object;

    .line 79
    .line 80
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 81
    .line 82
    .line 83
    move-result-object v1

    .line 84
    invoke-virtual {v0, v1}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setTOPIC_PLUS(Ljava/lang/String;)V

    .line 85
    .line 86
    .line 87
    sget v1, Lac/cloud/common/R$string;->topic_publish:I

    .line 88
    .line 89
    new-array v3, v2, [Ljava/lang/Object;

    .line 90
    .line 91
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 92
    .line 93
    .line 94
    move-result-object v1

    .line 95
    invoke-virtual {v0, v1}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setTOPIC_PUBLISH(Ljava/lang/String;)V

    .line 96
    .line 97
    .line 98
    sget v1, Lac/cloud/common/R$string;->dev_mqtt_host:I

    .line 99
    .line 100
    new-array v3, v2, [Ljava/lang/Object;

    .line 101
    .line 102
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 103
    .line 104
    .line 105
    move-result-object v1

    .line 106
    invoke-virtual {v0, v1}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setDEV_MQTT_HOST(Ljava/lang/String;)V

    .line 107
    .line 108
    .line 109
    sget v1, Lac/cloud/common/R$string;->test_mqtt_host:I

    .line 110
    .line 111
    new-array v3, v2, [Ljava/lang/Object;

    .line 112
    .line 113
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 114
    .line 115
    .line 116
    move-result-object v1

    .line 117
    invoke-virtual {v0, v1}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setTEST_MQTT_HOST(Ljava/lang/String;)V

    .line 118
    .line 119
    .line 120
    sget v1, Lac/cloud/common/R$string;->release_mqtt_host:I

    .line 121
    .line 122
    new-array v3, v2, [Ljava/lang/Object;

    .line 123
    .line 124
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 125
    .line 126
    .line 127
    move-result-object v3

    .line 128
    invoke-virtual {v0, v3}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setRELEASE_MQTT_HOST(Ljava/lang/String;)V

    .line 129
    .line 130
    .line 131
    new-array v3, v2, [Ljava/lang/Object;

    .line 132
    .line 133
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 134
    .line 135
    .line 136
    move-result-object v1

    .line 137
    invoke-virtual {v0, v1}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setPRE_RELEASE_MQTT_HOST(Ljava/lang/String;)V

    .line 138
    .line 139
    .line 140
    sget v1, Lac/cloud/common/R$string;->mqtt_server_port:I

    .line 141
    .line 142
    new-array v3, v2, [Ljava/lang/Object;

    .line 143
    .line 144
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 145
    .line 146
    .line 147
    move-result-object v1

    .line 148
    invoke-virtual {v0, v1}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->setMQTT_SERVER_PORT(Ljava/lang/String;)V

    .line 149
    .line 150
    .line 151
    sget-object v0, Lac/cloud/common/constant/ConfigConstants$DomainNameConfig;->Companion:Lac/cloud/common/constant/ConfigConstants$DomainNameConfig$Companion;

    .line 152
    .line 153
    sget v1, Lac/cloud/common/R$string;->test_domain_name_no_http:I

    .line 154
    .line 155
    new-array v3, v2, [Ljava/lang/Object;

    .line 156
    .line 157
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 158
    .line 159
    .line 160
    move-result-object v1

    .line 161
    invoke-virtual {v0, v1}, Lac/cloud/common/constant/ConfigConstants$DomainNameConfig$Companion;->setTEST_DOMAIN_NAME_NO_HTTP(Ljava/lang/String;)V

    .line 162
    .line 163
    .line 164
    sget v1, Lac/cloud/common/R$string;->old_test_domain_name_no_http:I

    .line 165
    .line 166
    new-array v3, v2, [Ljava/lang/Object;

    .line 167
    .line 168
    invoke-static {v1, v3}, Lac/cloud/uikit/ext/c;->i(I[Ljava/lang/Object;)Ljava/lang/String;

    .line 169
    .line 170
    .line 171
    move-result-object v1

    .line 172
    invoke-virtual {v0, v1}, Lac/cloud/common/constant/ConfigConstants$DomainNameConfig$Companion;->setOLD_TEST_DOMAIN_NAME_NO_HTTP(Ljava/lang/String;)V

## 4) MQTT config holder
.class public final Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;
.super Ljava/lang/Object;
.source "MqttServiceConstants.kt"


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x19
    name = "Companion"
.end annotation


# static fields
.field static final synthetic $$INSTANCE:Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

.field private static DEV_MQTT_HOST:Ljava/lang/String;

.field private static MQTT_SERVER_PORT:Ljava/lang/String;

.field private static PRE_RELEASE_MQTT_HOST:Ljava/lang/String;

.field private static RELEASE_MQTT_HOST:Ljava/lang/String;

.field private static TEST_MQTT_HOST:Ljava/lang/String;

.field private static TOPIC_PLUS:Ljava/lang/String;

.field private static TOPIC_PRINTER:Ljava/lang/String;

.field private static TOPIC_PUBLISH:Ljava/lang/String;

.field private static TOPIC_SERVER:Ljava/lang/String;


# direct methods
.method static constructor <clinit>()V
    .locals 1

    .line 1
    new-instance v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

    .line 2
    .line 3
    invoke-direct {v0}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;-><init>()V

    .line 4
    .line 5
    .line 6
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->$$INSTANCE:Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

    .line 7
    .line 8
    const-string v0, ""

    .line 9
    .line 10
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_PRINTER:Ljava/lang/String;

    .line 11
    .line 12
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_SERVER:Ljava/lang/String;

    .line 13
    .line 14
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_PLUS:Ljava/lang/String;

    .line 15
    .line 16
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_PUBLISH:Ljava/lang/String;

    .line 17
    .line 18
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TEST_MQTT_HOST:Ljava/lang/String;

    .line 19
    .line 20
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->PRE_RELEASE_MQTT_HOST:Ljava/lang/String;

    .line 21
    .line 22
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->RELEASE_MQTT_HOST:Ljava/lang/String;

    .line 23
    .line 24
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->DEV_MQTT_HOST:Ljava/lang/String;

    .line 25
    .line 26
    sput-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->MQTT_SERVER_PORT:Ljava/lang/String;

    .line 27
    .line 28
    return-void
.end method

.method private constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
.end method


# virtual methods
.method public final getDEV_MQTT_HOST()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->DEV_MQTT_HOST:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getMQTT_SERVER_PORT()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->MQTT_SERVER_PORT:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getPRE_RELEASE_MQTT_HOST()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->PRE_RELEASE_MQTT_HOST:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getRELEASE_MQTT_HOST()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->RELEASE_MQTT_HOST:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getTEST_MQTT_HOST()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TEST_MQTT_HOST:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getTOPIC_PLUS()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_PLUS:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getTOPIC_PRINTER()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_PRINTER:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getTOPIC_PUBLISH()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_PUBLISH:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final getTOPIC_SERVER()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->TOPIC_SERVER:Ljava/lang/String;

    .line 2
    .line 3
    return-object v0
.end method

.method public final setDEV_MQTT_HOST(Ljava/lang/String;)V
    .locals 1

    .line 1
    const-string v0, "<set-?>"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    sput-object p1, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->DEV_MQTT_HOST:Ljava/lang/String;

    .line 7
    .line 8
    return-void
.end method

.method public final setMQTT_SERVER_PORT(Ljava/lang/String;)V
    .locals 1

    .line 1
    const-string v0, "<set-?>"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    sput-object p1, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->MQTT_SERVER_PORT:Ljava/lang/String;

    .line 7
    .line 8
    return-void
.end method

.method public final setPRE_RELEASE_MQTT_HOST(Ljava/lang/String;)V
    .locals 1

    .line 1
    const-string v0, "<set-?>"

    .line 2
    .line 3

## 5) MQTT host selection by environment
.class public final Lcom/cloud/mqttservice/MQTTPropertiesUtil;
.super Ljava/lang/Object;
.source "MQTTPropertiesUtil.kt"


# static fields
.field public static final INSTANCE:Lcom/cloud/mqttservice/MQTTPropertiesUtil;


# direct methods
.method static constructor <clinit>()V
    .locals 1

    .line 1
    new-instance v0, Lcom/cloud/mqttservice/MQTTPropertiesUtil;

    .line 2
    .line 3
    invoke-direct {v0}, Lcom/cloud/mqttservice/MQTTPropertiesUtil;-><init>()V

    .line 4
    .line 5
    .line 6
    sput-object v0, Lcom/cloud/mqttservice/MQTTPropertiesUtil;->INSTANCE:Lcom/cloud/mqttservice/MQTTPropertiesUtil;

    .line 7
    .line 8
    return-void
.end method

.method private constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
.end method


# virtual methods
.method public final getMqttHost()Ljava/lang/String;
    .locals 1

    .line 1
    sget-object v0, Lp/a;->a:Lp/a;

    .line 2
    .line 3
    invoke-virtual {v0}, Lp/a;->a()Lac/cloud/reposervice/service/a;

    .line 4
    .line 5
    .line 6
    move-result-object v0

    .line 7
    invoke-virtual {v0}, Lac/cloud/reposervice/service/a;->b()Z

    .line 8
    .line 9
    .line 10
    move-result v0

    .line 11
    if-nez v0, :cond_0

    .line 12
    .line 13
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig;->Companion:Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

    .line 14
    .line 15
    invoke-virtual {v0}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->getTEST_MQTT_HOST()Ljava/lang/String;

    .line 16
    .line 17
    .line 18
    move-result-object v0

    .line 19
    goto :goto_0

    .line 20
    :cond_0
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig;->Companion:Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

    .line 21
    .line 22
    invoke-virtual {v0}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->getRELEASE_MQTT_HOST()Ljava/lang/String;

    .line 23
    .line 24
    .line 25
    move-result-object v0

    .line 26
    :goto_0
    return-object v0
.end method

## 6) Connection defaults + credentials generation
.class public final Lcom/cloud/mqttservice/model/ConnectionModel;
.super Ljava/lang/Object;
.source "ConnectionModel.kt"


# static fields
.field public static final INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

.field private static clientHandle:Ljava/lang/String;

.field private static fileName:Ljava/lang/String;

.field private static isAutomaticReconnect:Z

.field private static isCleanSession:Z

.field private static isTlsConnection:Z

.field private static keepAlive:I

.field private static mqtt_version:I

.field private static serverHostName:Ljava/lang/String;

.field private static serverPort:I

.field private static timeout:I


# direct methods
.method static constructor <clinit>()V
    .locals 1

    .line 1
    new-instance v0, Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 2
    .line 3
    invoke-direct {v0}, Lcom/cloud/mqttservice/model/ConnectionModel;-><init>()V

    .line 4
    .line 5
    .line 6
    sput-object v0, Lcom/cloud/mqttservice/model/ConnectionModel;->INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 7
    .line 8
    const-string v0, "ANYCUBIC_MQTT_CLIENT_HANDLE"

    .line 9
    .line 10
    sput-object v0, Lcom/cloud/mqttservice/model/ConnectionModel;->clientHandle:Ljava/lang/String;

    .line 11
    .line 12
    sget-object v0, Lcom/cloud/mqttservice/MQTTPropertiesUtil;->INSTANCE:Lcom/cloud/mqttservice/MQTTPropertiesUtil;

    .line 13
    .line 14
    invoke-virtual {v0}, Lcom/cloud/mqttservice/MQTTPropertiesUtil;->getMqttHost()Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v0

    .line 18
    sput-object v0, Lcom/cloud/mqttservice/model/ConnectionModel;->serverHostName:Ljava/lang/String;

    .line 19
    .line 20
    sget-object v0, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig;->Companion:Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

    .line 21
    .line 22
    invoke-virtual {v0}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->getMQTT_SERVER_PORT()Ljava/lang/String;

    .line 23
    .line 24
    .line 25
    move-result-object v0

    .line 26
    invoke-static {v0}, Ljava/lang/Integer;->parseInt(Ljava/lang/String;)I

    .line 27
    .line 28
    .line 29
    move-result v0

    .line 30
    sput v0, Lcom/cloud/mqttservice/model/ConnectionModel;->serverPort:I

    .line 31
    .line 32
    const/4 v0, 0x1

    .line 33
    sput-boolean v0, Lcom/cloud/mqttservice/model/ConnectionModel;->isCleanSession:Z

    .line 34
    .line 35
    sput-boolean v0, Lcom/cloud/mqttservice/model/ConnectionModel;->isAutomaticReconnect:Z

    .line 36
    .line 37
    sput-boolean v0, Lcom/cloud/mqttservice/model/ConnectionModel;->isTlsConnection:Z

    .line 38
    .line 39
    const-string v0, "AnyCubicAPP.bks"

    .line 40
    .line 41
    sput-object v0, Lcom/cloud/mqttservice/model/ConnectionModel;->fileName:Ljava/lang/String;

    .line 42
    .line 43
    const/16 v0, 0xa

    .line 44
    .line 45
    sput v0, Lcom/cloud/mqttservice/model/ConnectionModel;->timeout:I

    .line 46
    .line 47
    const/16 v0, 0xc8

    .line 48
    .line 49
    sput v0, Lcom/cloud/mqttservice/model/ConnectionModel;->keepAlive:I

    .line 50
    .line 51
    const/4 v0, 0x4

    .line 52
    sput v0, Lcom/cloud/mqttservice/model/ConnectionModel;->mqtt_version:I

    .line 53
    .line 54
    return-void
.end method

.method private constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
.end method

.method private final encryptWithPublicKey(Ljava/security/PublicKey;Ljava/lang/String;)Ljava/lang/String;
    .locals 2

    .line 1
    :try_start_0
    const-string v0, "RSA/ECB/PKCS1Padding"

    .line 2
    .line 3
    invoke-static {v0}, Ljavax/crypto/Cipher;->getInstance(Ljava/lang/String;)Ljavax/crypto/Cipher;

    .line 4
    .line 5
    .line 6
    move-result-object v0

    .line 7
    const/4 v1, 0x1

    .line 8
    invoke-virtual {v0, v1, p1}, Ljavax/crypto/Cipher;->init(ILjava/security/Key;)V

    .line 9
    .line 10
    .line 11
    const-string p1, "UTF-8"

    .line 12
    .line 13
    invoke-static {p1}, Ljava/nio/charset/Charset;->forName(Ljava/lang/String;)Ljava/nio/charset/Charset;

    .line 14
    .line 15
    .line 16
    move-result-object p1

    .line 17
    const-string v1, "forName(...)"

    .line 18
    .line 19
    invoke-static {p1, v1}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 20
    .line 21
    .line 22
    invoke-virtual {p2, p1}, Ljava/lang/String;->getBytes(Ljava/nio/charset/Charset;)[B

    .line 23
    .line 24
    .line 25
    move-result-object p1

    .line 26
    const-string p2, "getBytes(...)"

    .line 27
    .line 28
    invoke-static {p1, p2}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 29
    .line 30
    .line 31
    invoke-virtual {v0, p1}, Ljavax/crypto/Cipher;->doFinal([B)[B

.method public final getMultiPointClientId()Ljava/lang/String;
    .locals 4

    .line 1
    sget-object v0, Lh2/a;->a:Lh2/a;

    .line 2
    .line 3
    invoke-virtual {v0}, Lh2/a;->q()Ljava/lang/String;

    .line 4
    .line 5
    .line 6
    move-result-object v0

    .line 7
    invoke-static {v0}, Lcom/blankj/utilcode/util/l;->b(Ljava/lang/String;)Ljava/lang/String;

    .line 8
    .line 9
    .line 10
    move-result-object v1

    .line 11
    const-string v2, "encryptMD5ToString(...)"

    .line 12
    .line 13
    invoke-static {v1, v2}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 14
    .line 15
    .line 16
    sget-object v2, Ljava/util/Locale;->ROOT:Ljava/util/Locale;

    .line 17
    .line 18
    invoke-virtual {v1, v2}, Ljava/lang/String;->toLowerCase(Ljava/util/Locale;)Ljava/lang/String;

    .line 19
    .line 20
    .line 21
    move-result-object v1

    .line 22
    const-string v2, "toLowerCase(...)"

    .line 23
    .line 24
    invoke-static {v1, v2}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 25
    .line 26
    .line 27
    new-instance v2, Ljava/lang/StringBuilder;

    .line 28
    .line 29
    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    .line 30
    .line 31
    .line 32
    const-string v3, "android_"

    .line 33
    .line 34
    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 35
    .line 36
    .line 37
    invoke-virtual {v2, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 38
    .line 39
    .line 40
    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 41
    .line 42
    .line 43
    move-result-object v1

    .line 44
    new-instance v2, Ljava/lang/StringBuilder;

    .line 45
    .line 46
    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    .line 47
    .line 48
    .line 49
    const-string v3, "MQTT token: "

    .line 50
    .line 51
    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 52
    .line 53
    .line 54
    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 55
    .line 56
    .line 57
    const-string v0, ", clientId: "

    .line 58
    .line 59
    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 60
    .line 61
    .line 62
    invoke-virtual {v2, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 63
    .line 64
    .line 65
    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 66
    .line 67
    .line 68
    move-result-object v0

    .line 69
    invoke-static {v0}, Lcom/cloud/mqttservice/internal/ActionListenerKt;->logd(Ljava/lang/String;)V

    .line 70
    .line 71
    .line 72
    return-object v1
.end method

.method public final getLoginInfo(Landroid/content/Context;)Lh8/j;
    .locals 5
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Landroid/content/Context;",
            ")",
            "Lh8/j<",
            "Ljava/lang/String;",
            "Ljava/lang/String;",
            ">;"
        }
    .end annotation

    .line 1
    const-string v0, "context"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    sget-object v0, Lh2/a;->a:Lh2/a;

    .line 7
    .line 8
    invoke-virtual {v0}, Lh2/a;->r()Lac/cloud/publicbean/mine/User;

    .line 9
    .line 10
    .line 11
    move-result-object v1

    .line 12
    const/4 v2, 0x0

    .line 13
    if-eqz v1, :cond_0

    .line 14
    .line 15
    invoke-virtual {v1}, Lac/cloud/publicbean/mine/User;->getUser_email()Ljava/lang/String;

    .line 16
    .line 17
    .line 18
    move-result-object v1

    .line 19
    goto :goto_0

    .line 20
    :cond_0
    move-object v1, v2

    .line 21
    :goto_0
    invoke-virtual {v0}, Lh2/a;->r()Lac/cloud/publicbean/mine/User;

    .line 22
    .line 23
    .line 24
    move-result-object v3

    .line 25
    if-eqz v3, :cond_1

    .line 26
    .line 27
    invoke-virtual {v3}, Lac/cloud/publicbean/mine/User;->getMobile()Ljava/lang/String;

    .line 28
    .line 29
    .line 30
    move-result-object v2

    .line 31
    :cond_1
    if-eqz v1, :cond_2

    .line 32
    .line 33
    invoke-interface {v1}, Ljava/lang/CharSequence;->length()I

    .line 34
    .line 35
    .line 36
    move-result v3

    .line 37
    if-nez v3, :cond_5

    .line 38
    .line 39
    :cond_2
    if-eqz v2, :cond_4

    .line 40
    .line 41
    invoke-interface {v2}, Ljava/lang/CharSequence;->length()I

    .line 42
    .line 43
    .line 44
    move-result v1

    .line 45
    if-nez v1, :cond_3

    .line 46
    .line 47
    goto :goto_1

    .line 48
    :cond_3
    move-object v1, v2

    .line 49
    goto :goto_2

    .line 50
    :cond_4
    :goto_1
    const-string v1, ""

    .line 51
    .line 52
    :cond_5
    :goto_2
    invoke-virtual {v0}, Lh2/a;->q()Ljava/lang/String;

    .line 53
    .line 54
    .line 55
    move-result-object v0

    .line 56
    invoke-direct {p0, p1}, Lcom/cloud/mqttservice/model/ConnectionModel;->getPublicKeyFromCert(Landroid/content/Context;)Ljava/security/PublicKey;

    .line 57
    .line 58
    .line 59
    move-result-object p1

    .line 60
    invoke-direct {p0, p1, v0}, Lcom/cloud/mqttservice/model/ConnectionModel;->encryptWithPublicKey(Ljava/security/PublicKey;Ljava/lang/String;)Ljava/lang/String;

    .line 61
    .line 62
    .line 63
    move-result-object p1

    .line 64
    invoke-virtual {p0}, Lcom/cloud/mqttservice/model/ConnectionModel;->getMultiPointClientId()Ljava/lang/String;

    .line 65
    .line 66
    .line 67
    move-result-object v2

    .line 68
    new-instance v3, Ljava/lang/StringBuilder;

    .line 69
    .line 70
    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    .line 71
    .line 72
    .line 73
    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 74
    .line 75
    .line 76
    invoke-virtual {v3, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 77
    .line 78
    .line 79
    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 80
    .line 81
    .line 82
    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 83
    .line 84
    .line 85
    move-result-object v2

    .line 86
    invoke-static {v2}, Lcom/blankj/utilcode/util/l;->b(Ljava/lang/String;)Ljava/lang/String;

    .line 87
    .line 88
    .line 89
    move-result-object v2

    .line 90
    const-string v3, "encryptMD5ToString(...)"

    .line 91
    .line 92
    invoke-static {v2, v3}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 93
    .line 94
    .line 95
    sget-object v3, Ljava/util/Locale;->ROOT:Ljava/util/Locale;

    .line 96
    .line 97
    invoke-virtual {v2, v3}, Ljava/lang/String;->toLowerCase(Ljava/util/Locale;)Ljava/lang/String;

    .line 98
    .line 99
    .line 100
    move-result-object v2

    .line 101
    const-string v3, "toLowerCase(...)"

    .line 102
    .line 103
    invoke-static {v2, v3}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 104
    .line 105
    .line 106
    new-instance v3, Ljava/lang/StringBuilder;

    .line 107
    .line 108
    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    .line 109
    .line 110
    .line 111
    const-string v4, "user|app|"

    .line 112
    .line 113
    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 114
    .line 115
    .line 116
    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 117
    .line 118
    .line 119
    const/16 v4, 0x7c

    .line 120
    .line 121
    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 122
    .line 123
    .line 124
    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 125
    .line 126
    .line 127
    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 128
    .line 129
    .line 130
    move-result-object v2

    .line 131
    new-instance v3, Ljava/lang/StringBuilder;

    .line 132
    .line 133
    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    .line 134
    .line 135
    .line 136
    const-string v4, "MQTT \u767b\u5f55\u4fe1\u606f:\u767b\u5f55\u7528\u6237\u540d:"

    .line 137
    .line 138
    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 139
    .line 140
    .line 141
    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 142
    .line 143
    .line 144
    const-string v1, ",\u7528\u6237\u540d:"

    .line 145
    .line 146
    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 147
    .line 148
    .line 149
    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 150
    .line 151
    .line 152
    const-string v1, ",\u5bc6\u7801:"

    .line 153
    .line 154
    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 155
    .line 156
    .line 157
    invoke-virtual {v3, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 158
    .line 159
    .line 160
    const-string v1, ",token:"

    .line 161
    .line 162
    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 163
    .line 164
    .line 165
    invoke-virtual {v3, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 166
    .line 167
    .line 168
    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 169
    .line 170
    .line 171
    move-result-object v0

    .line 172
    invoke-static {v0}, Lcom/cloud/mqttservice/internal/ActionListenerKt;->loge(Ljava/lang/String;)V

    .line 173
    .line 174
    .line 175
    new-instance v0, Lh8/j;

    .line 176
    .line 177
    invoke-direct {v0, v2, p1}, Lh8/j;-><init>(Ljava/lang/Object;Ljava/lang/Object;)V

    .line 178
    .line 179
    .line 180
    return-object v0
.end method

## 7) Connection creation (ssl:// or tcp://) + connect options
.class public final Lcom/cloud/mqttservice/internal/Connection$Companion;
.super Ljava/lang/Object;
.source "Connection.kt"


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/cloud/mqttservice/internal/Connection;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x19
    name = "Companion"
.end annotation


# direct methods
.method private constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public synthetic constructor <init>(Lkotlin/jvm/internal/g;)V
    .locals 0

    .line 2
    invoke-direct {p0}, Lcom/cloud/mqttservice/internal/Connection$Companion;-><init>()V

    return-void
.end method

.method public static synthetic createConnection$default(Lcom/cloud/mqttservice/internal/Connection$Companion;Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IZILjava/lang/Object;)Lcom/cloud/mqttservice/internal/Connection;
    .locals 7

    .line 1
    and-int/lit8 p8, p7, 0x4

    .line 2
    .line 3
    if-eqz p8, :cond_0

    .line 4
    .line 5
    sget-object p3, Lcom/cloud/mqttservice/model/ConnectionModel;->INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 6
    .line 7
    invoke-virtual {p3}, Lcom/cloud/mqttservice/model/ConnectionModel;->getMultiPointClientId()Ljava/lang/String;

    .line 8
    .line 9
    .line 10
    move-result-object p3

    .line 11
    :cond_0
    move-object v3, p3

    .line 12
    and-int/lit8 p3, p7, 0x8

    .line 13
    .line 14
    if-eqz p3, :cond_1

    .line 15
    .line 16
    sget-object p3, Lcom/cloud/mqttservice/model/ConnectionModel;->INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 17
    .line 18
    invoke-virtual {p3}, Lcom/cloud/mqttservice/model/ConnectionModel;->getServerHostName()Ljava/lang/String;

    .line 19
    .line 20
    .line 21
    move-result-object p4

    .line 22
    :cond_1
    move-object v4, p4

    .line 23
    and-int/lit8 p3, p7, 0x10

    .line 24
    .line 25
    if-eqz p3, :cond_2

    .line 26
    .line 27
    sget-object p3, Lcom/cloud/mqttservice/model/ConnectionModel;->INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 28
    .line 29
    invoke-virtual {p3}, Lcom/cloud/mqttservice/model/ConnectionModel;->getServerPort()I

    .line 30
    .line 31
    .line 32
    move-result p5

    .line 33
    :cond_2
    move v5, p5

    .line 34
    and-int/lit8 p3, p7, 0x20

    .line 35
    .line 36
    if-eqz p3, :cond_3

    .line 37
    .line 38
    sget-object p3, Lcom/cloud/mqttservice/model/ConnectionModel;->INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 39
    .line 40
    invoke-virtual {p3}, Lcom/cloud/mqttservice/model/ConnectionModel;->isTlsConnection()Z

    .line 41
    .line 42
    .line 43
    move-result p6

    .line 44
    :cond_3
    move v6, p6

    .line 45
    move-object v0, p0

    .line 46
    move-object v1, p1

    .line 47
    move-object v2, p2

    .line 48
    invoke-virtual/range {v0 .. v6}, Lcom/cloud/mqttservice/internal/Connection$Companion;->createConnection(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IZ)Lcom/cloud/mqttservice/internal/Connection;

    .line 49
    .line 50
    .line 51
    move-result-object p0

    .line 52
    return-object p0
.end method


# virtual methods
.method public final createConnection(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IZ)Lcom/cloud/mqttservice/internal/Connection;
    .locals 17

    .line 1
    move-object/from16 v7, p1

    .line 2
    .line 3
    move-object/from16 v8, p3

    .line 4
    .line 5
    move-object/from16 v9, p4

    .line 6
    .line 7
    move/from16 v10, p5

    .line 8
    .line 9
    const-string v0, "context"

    .line 10
    .line 11
    invoke-static {v7, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 12
    .line 13
    .line 14
    const-string v0, "clientHandle"

    .line 15
    .line 16
    move-object/from16 v11, p2

    .line 17
    .line 18
    invoke-static {v11, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 19
    .line 20
    .line 21
    const-string v0, "clientId"

    .line 22
    .line 23
    invoke-static {v8, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 24
    .line 25
    .line 26
    const-string v0, "host"

    .line 27
    .line 28
    invoke-static {v9, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 29
    .line 30
    .line 31
    const/16 v0, 0x3a

    .line 32
    .line 33
    if-eqz p6, :cond_0

    .line 34
    .line 35
    new-instance v1, Ljava/lang/StringBuilder;

    .line 36
    .line 37
    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    .line 38
    .line 39
    .line 40
    const-string v2, "ssl://"

    .line 41
    .line 42
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 43
    .line 44
    .line 45
    invoke-virtual {v1, v9}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 46
    .line 47
    .line 48
    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 49
    .line 50
    .line 51
    invoke-virtual {v1, v10}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    .line 52
    .line 53
    .line 54
    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 55
    .line 56
    .line 57
    move-result-object v0

    .line 58
    :goto_0
    move-object v2, v0

    .line 59
    goto :goto_1

    .line 60
    :cond_0
    new-instance v1, Ljava/lang/StringBuilder;

    .line 61
    .line 62
    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    .line 63
    .line 64
    .line 65
    const-string v2, "tcp://"

    .line 66
    .line 67
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 68
    .line 69
    .line 70
    invoke-virtual {v1, v9}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 71
    .line 72
    .line 73
    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 74
    .line 75
    .line 76
    invoke-virtual {v1, v10}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    .line 77
    .line 78
    .line 79
    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 80
    .line 81
    .line 82
    move-result-object v0

    .line 83
    goto :goto_0

    .line 84
    :goto_1
    new-instance v12, Ltb/l;

    .line 85
    .line 86
    invoke-direct {v12}, Ltb/l;-><init>()V

    .line 87
    .line 88
    .line 89
    sget-object v0, Lcom/cloud/mqttservice/internal/Notify;->INSTANCE:Lcom/cloud/mqttservice/internal/Notify;

    .line 90
    .line 91
    const/4 v1, 0x0

    .line 92
    sget v3, Lcom/cloud/mqttservice/R$string;->common_other:I

    .line 93
    .line 94
    invoke-virtual {v0, v7, v8, v1, v3}, Lcom/cloud/mqttservice/internal/Notify;->foregroundNotification(Landroid/content/Context;Ljava/lang/String;Landroid/content/Intent;I)Landroid/app/Notification;

    .line 95
    .line 96
    .line 97
    move-result-object v13

    .line 98
    sget-object v14, Lcom/cloud/mqttservice/model/ConnectionModel;->INSTANCE:Lcom/cloud/mqttservice/model/ConnectionModel;

    .line 99
    .line 100
    invoke-virtual {v14, v7}, Lcom/cloud/mqttservice/model/ConnectionModel;->getLoginInfo(Landroid/content/Context;)Lh8/j;

    .line 101
    .line 102
    .line 103
    move-result-object v15

    .line 104
    new-instance v6, Lcom/cloud/mqttservice/MqttAndroidClient;

    .line 105
    .line 106
    const/16 v5, 0x8

    .line 107
    .line 108
    const/16 v16, 0x0

    .line 109
    .line 110
    const/4 v4, 0x0

    .line 111
    move-object v0, v6

    .line 112
    move-object/from16 v1, p1

    .line 113
    .line 114
    move-object/from16 v3, p3

    .line 115
    .line 116
    move-object v7, v6

    .line 117
    move-object/from16 v6, v16

    .line 118
    .line 119
    invoke-direct/range {v0 .. v6}, Lcom/cloud/mqttservice/MqttAndroidClient;-><init>(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Lcom/cloud/mqttservice/Ack;ILkotlin/jvm/internal/g;)V

    .line 120
    .line 121
    .line 122
    invoke-virtual {v14}, Lcom/cloud/mqttservice/model/ConnectionModel;->isCleanSession()Z

    .line 123
    .line 124
    .line 125
    move-result v0

    .line 126
    invoke-virtual {v12, v0}, Ltb/l;->t(Z)V

    .line 127
    .line 128
    .line 129
    invoke-virtual {v14}, Lcom/cloud/mqttservice/model/ConnectionModel;->isAutomaticReconnect()Z

    .line 130
    .line 131
    .line 132
    move-result v0

    .line 133
    invoke-virtual {v12, v0}, Ltb/l;->s(Z)V

    .line 134
    .line 135
    .line 136
    invoke-virtual {v14}, Lcom/cloud/mqttservice/model/ConnectionModel;->getTimeout()I

    .line 137
    .line 138
    .line 139
    move-result v0

    .line 140
    invoke-virtual {v12, v0}, Ltb/l;->u(I)V

    .line 141
    .line 142
    .line 143
    invoke-virtual {v14}, Lcom/cloud/mqttservice/model/ConnectionModel;->getKeepAlive()I

    .line 144
    .line 145
    .line 146
    move-result v0

    .line 147
    invoke-virtual {v12, v0}, Ltb/l;->w(I)V

    .line 148
    .line 149
    .line 150
    invoke-virtual {v14}, Lcom/cloud/mqttservice/model/ConnectionModel;->getMqtt_version()I

    .line 151
    .line 152
    .line 153
    move-result v0

    .line 154
    invoke-virtual {v12, v0}, Ltb/l;->x(I)V

    .line 155
    .line 156
    .line 157
    invoke-virtual {v15}, Lh8/j;->c()Ljava/lang/Object;

    .line 158
    .line 159
    .line 160
    move-result-object v0

    .line 161
    check-cast v0, Ljava/lang/String;

    .line 162
    .line 163
    invoke-virtual {v12, v0}, Ltb/l;->A(Ljava/lang/String;)V

    .line 164
    .line 165
    .line 166
    invoke-virtual {v15}, Lh8/j;->d()Ljava/lang/Object;

    .line 167
    .line 168
    .line 169
    move-result-object v0

    .line 170
    check-cast v0, Ljava/lang/String;

    .line 171
    .line 172
    invoke-virtual {v0}, Ljava/lang/String;->toCharArray()[C

    .line 173
    .line 174
    .line 175
    move-result-object v0

    .line 176
    const-string v1, "toCharArray(...)"

    .line 177
    .line 178
    invoke-static {v0, v1}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 179
    .line 180
    .line 181
    invoke-virtual {v12, v0}, Ltb/l;->y([C)V

    .line 182
    .line 183
    .line 184
    const/4 v0, 0x0

    .line 185
    invoke-virtual {v12, v0}, Ltb/l;->v(Z)V

    .line 186
    .line 187
    .line 188
    if-eqz p6, :cond_1

    .line 189
    .line 190
    new-instance v0, Ljava/lang/StringBuilder;

    .line 191
    .line 192
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    .line 193
    .line 194
    .line 195
    const-string v1, "MQTT:\u8fde\u63a5:"

    .line 196
    .line 197
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 198
    .line 199
    .line 200
    invoke-virtual {v12}, Ltb/l;->m()Ljava/lang/String;

    .line 201
    .line 202
    .line 203
    move-result-object v1

    .line 204
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 205
    .line 206
    .line 207
    const/16 v1, 0x2c

    .line 208
    .line 209
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 210
    .line 211
    .line 212
    invoke-virtual {v12}, Ltb/l;->h()[C

    .line 213
    .line 214
    .line 215
    move-result-object v1

    .line 216
    const-string v2, "getPassword(...)"

    .line 217
    .line 218
    invoke-static {v1, v2}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 219
    .line 220
    .line 221
    new-instance v2, Ljava/lang/String;

    .line 222
    .line 223
    invoke-direct {v2, v1}, Ljava/lang/String;-><init>([C)V

    .line 224
    .line 225
    .line 226
    invoke-virtual {v0, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 227
    .line 228
    .line 229
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 230
    .line 231
    .line 232
    move-result-object v0

    .line 233
    invoke-static {v0}, Lcom/cloud/mqttservice/internal/ActionListenerKt;->logd(Ljava/lang/String;)V

    .line 234
    .line 235
    .line 236
    invoke-virtual/range {p1 .. p1}, Landroid/content/Context;->getResources()Landroid/content/res/Resources;

    .line 237
    .line 238
    .line 239
    move-result-object v0

    .line 240
    sget v1, Lcom/cloud/mqttservice/R$raw;->ca:I

    .line 241
    .line 242
    invoke-virtual {v0, v1}, Landroid/content/res/Resources;->openRawResource(I)Ljava/io/InputStream;

    .line 243
    .line 244
    .line 245
    move-result-object v0

    .line 246
    invoke-virtual {v7, v0}, Lcom/cloud/mqttservice/MqttAndroidClient;->getSingleSocketFactory(Ljava/io/InputStream;)Ljavax/net/ssl/SSLSocketFactory;

    .line 247
    .line 248
    .line 249
    move-result-object v0

    .line 250
    invoke-virtual {v12, v0}, Ltb/l;->z(Ljavax/net/SocketFactory;)V

    .line 251
    .line 252
    .line 253
    :cond_1
    sget v0, Landroid/os/Build$VERSION;->SDK_INT:I

    .line 254
    .line 255
    const/16 v1, 0x1a

    .line 256
    .line 257
    if-lt v0, v1, :cond_2

    .line 258
    .line 259
    const/16 v0, 0x4d

    .line 260
    .line 261
    invoke-virtual {v7, v13, v0}, Lcom/cloud/mqttservice/MqttAndroidClient;->setForegroundService(Landroid/app/Notification;I)V

    .line 262
    .line 263
    .line 264
    :cond_2
    new-instance v0, Ljava/lang/StringBuilder;

    .line 265
    .line 266
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    .line 267
    .line 268
    .line 269
    const-string v1, "clientId "

    .line 270
    .line 271
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 272
    .line 273
    .line 274
    invoke-virtual {v0, v8}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 275
    .line 276
    .line 277
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 278
    .line 279
    .line 280
    move-result-object v0

    .line 281
    invoke-static {v0}, Lcom/cloud/mqttservice/internal/ActionListenerKt;->logd(Ljava/lang/String;)V

    .line 282
    .line 283
    .line 284
    new-instance v13, Lcom/cloud/mqttservice/internal/Connection;

    .line 285
    .line 286
    const/4 v14, 0x0

    .line 287
    move-object v0, v13

    .line 288
    move-object/from16 v1, p2

    .line 289
    .line 290
    move-object/from16 v2, p3

    .line 291
    .line 292
    move-object/from16 v3, p4

    .line 293
    .line 294
    move/from16 v4, p5

    .line 295
    .line 296
    move-object/from16 v5, p1

    .line 297
    .line 298
    move-object v6, v7

    .line 299
    move/from16 v7, p6

    .line 300
    .line 301
    move-object v8, v12

    .line 302
    move-object v9, v14

    .line 303
    invoke-direct/range {v0 .. v9}, Lcom/cloud/mqttservice/internal/Connection;-><init>(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ILandroid/content/Context;Lcom/cloud/mqttservice/MqttAndroidClient;ZLtb/l;Lkotlin/jvm/internal/g;)V

    .line 304
    .line 305
    .line 306
    return-object v13
.end method

## 8) TLS socket factory + custom trust manager
.method public final getSingleSocketFactory(Ljava/io/InputStream;)Ljavax/net/ssl/SSLSocketFactory;
    .locals 5
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/lang/Exception;
        }
    .end annotation

    .line 1
    const/4 v0, 0x0

    .line 2
    new-instance v1, Lua/a;

    .line 3
    .line 4
    invoke-direct {v1}, Lua/a;-><init>()V

    .line 5
    .line 6
    .line 7
    invoke-static {v1}, Ljava/security/Security;->addProvider(Ljava/security/Provider;)I

    .line 8
    .line 9
    .line 10
    new-instance v1, Ljava/util/ArrayList;

    .line 11
    .line 12
    invoke-direct {v1}, Ljava/util/ArrayList;-><init>()V

    .line 13
    .line 14
    .line 15
    const-string v2, "X.509"

    .line 16
    .line 17
    invoke-static {v2}, Ljava/security/cert/CertificateFactory;->getInstance(Ljava/lang/String;)Ljava/security/cert/CertificateFactory;

    .line 18
    .line 19
    .line 20
    move-result-object v2

    .line 21
    new-instance v3, Ljava/io/BufferedInputStream;

    .line 22
    .line 23
    invoke-direct {v3, p1}, Ljava/io/BufferedInputStream;-><init>(Ljava/io/InputStream;)V

    .line 24
    .line 25
    .line 26
    :goto_0
    invoke-virtual {v3}, Ljava/io/BufferedInputStream;->available()I

    .line 27
    .line 28
    .line 29
    move-result p1

    .line 30
    if-lez p1, :cond_0

    .line 31
    .line 32
    invoke-virtual {v2, v3}, Ljava/security/cert/CertificateFactory;->generateCertificate(Ljava/io/InputStream;)Ljava/security/cert/Certificate;

    .line 33
    .line 34
    .line 35
    move-result-object p1

    .line 36
    const-string v4, "null cannot be cast to non-null type java.security.cert.X509Certificate"

    .line 37
    .line 38
    invoke-static {p1, v4}, Lkotlin/jvm/internal/m;->d(Ljava/lang/Object;Ljava/lang/String;)V

    .line 39
    .line 40
    .line 41
    check-cast p1, Ljava/security/cert/X509Certificate;

    .line 42
    .line 43
    invoke-interface {v1, p1}, Ljava/util/List;->add(Ljava/lang/Object;)Z

    .line 44
    .line 45
    .line 46
    goto :goto_0

    .line 47
    :cond_0
    invoke-virtual {v3}, Ljava/io/BufferedInputStream;->reset()V

    .line 48
    .line 49
    .line 50
    new-instance p1, Lcom/cloud/mqttservice/MqttAndroidClient$MQTTTrustManager;

    .line 51
    .line 52
    new-array v2, v0, [Ljava/security/cert/X509Certificate;

    .line 53
    .line 54
    invoke-interface {v1, v2}, Ljava/util/Collection;->toArray([Ljava/lang/Object;)[Ljava/lang/Object;

    .line 55
    .line 56
    .line 57
    move-result-object v1

    .line 58
    check-cast v1, [Ljava/security/cert/X509Certificate;

    .line 59
    .line 60
    invoke-direct {p1, p0, v1}, Lcom/cloud/mqttservice/MqttAndroidClient$MQTTTrustManager;-><init>(Lcom/cloud/mqttservice/MqttAndroidClient;[Ljava/security/cert/X509Certificate;)V

    .line 61
    .line 62
    .line 63
    const-string v1, "TLSv1.2"

    .line 64
    .line 65
    invoke-static {v1}, Ljavax/net/ssl/SSLContext;->getInstance(Ljava/lang/String;)Ljavax/net/ssl/SSLContext;

    .line 66
    .line 67
    .line 68
    move-result-object v1

    .line 69
    const/4 v2, 0x1

    .line 70
    new-array v2, v2, [Ljavax/net/ssl/TrustManager;

    .line 71
    .line 72
    aput-object p1, v2, v0

    .line 73
    .line 74
    const/4 p1, 0x0

    .line 75
    invoke-virtual {v1, p1, v2, p1}, Ljavax/net/ssl/SSLContext;->init([Ljavax/net/ssl/KeyManager;[Ljavax/net/ssl/TrustManager;Ljava/security/SecureRandom;)V

    .line 76
    .line 77
    .line 78
    invoke-virtual {v1}, Ljavax/net/ssl/SSLContext;->getSocketFactory()Ljavax/net/ssl/SSLSocketFactory;

    .line 79
    .line 80
    .line 81
    move-result-object p1

    .line 82
    return-object p1
.end method

.class public final Lcom/cloud/mqttservice/MqttAndroidClient$MQTTTrustManager;
.super Ljava/lang/Object;
.source "MqttAndroidClient.kt"

# interfaces
.implements Ljavax/net/ssl/X509TrustManager;


# annotations
.annotation build Landroid/annotation/SuppressLint;
    value = {
        "CustomX509TrustManager"
    }
.end annotation

.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/cloud/mqttservice/MqttAndroidClient;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x11
    name = "MQTTTrustManager"
.end annotation


# instance fields
.field private final certificates:[Ljava/security/cert/X509Certificate;

.field final synthetic this$0:Lcom/cloud/mqttservice/MqttAndroidClient;


# direct methods
.method public constructor <init>(Lcom/cloud/mqttservice/MqttAndroidClient;[Ljava/security/cert/X509Certificate;)V
    .locals 1
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "([",
            "Ljava/security/cert/X509Certificate;",
            ")V"
        }
    .end annotation

    .line 1
    const-string v0, "certificates"

    .line 2
    .line 3
    invoke-static {p2, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    iput-object p1, p0, Lcom/cloud/mqttservice/MqttAndroidClient$MQTTTrustManager;->this$0:Lcom/cloud/mqttservice/MqttAndroidClient;

    .line 7
    .line 8
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 9
    .line 10
    .line 11
    iput-object p2, p0, Lcom/cloud/mqttservice/MqttAndroidClient$MQTTTrustManager;->certificates:[Ljava/security/cert/X509Certificate;

    .line 12
    .line 13
    return-void
.end method


# virtual methods
.method public checkClientTrusted([Ljava/security/cert/X509Certificate;Ljava/lang/String;)V
    .locals 0
    .annotation build Landroid/annotation/SuppressLint;
        value = {
            "TrustAllX509TrustManager"
        }
    .end annotation

    .line 1
    return-void
.end method

.method public checkServerTrusted([Ljava/security/cert/X509Certificate;Ljava/lang/String;)V
    .locals 1

    .line 1
    if-eqz p1, :cond_2

    .line 2
    .line 3
    invoke-static {p1}, Lkotlin/jvm/internal/b;->a([Ljava/lang/Object;)Ljava/util/Iterator;

    .line 4
    .line 5
    .line 6
    move-result-object p1

    .line 7
    :cond_0
    invoke-interface {p1}, Ljava/util/Iterator;->hasNext()Z

    .line 8
    .line 9
    .line 10
    move-result p2

    .line 11
    if-eqz p2, :cond_1

    .line 12
    .line 13
    invoke-interface {p1}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    .line 14
    .line 15
    .line 16
    move-result-object p2

    .line 17
    check-cast p2, Ljava/security/cert/X509Certificate;

    .line 18
    .line 19
    iget-object v0, p0, Lcom/cloud/mqttservice/MqttAndroidClient$MQTTTrustManager;->certificates:[Ljava/security/cert/X509Certificate;

    .line 20
    .line 21
    invoke-static {v0, p2}, Lkotlin/collections/k;->q([Ljava/lang/Object;Ljava/lang/Object;)Z

    .line 22
    .line 23
    .line 24
    move-result p2

    .line 25
    if-eqz p2, :cond_0

    .line 26
    .line 27
    goto :goto_0

    .line 28
    :cond_1
    new-instance p1, Ljava/security/cert/CertificateException;

    .line 29
    .line 30
    const-string p2, "MQTT-Error Untrusted server certificate"

    .line 31
    .line 32
    invoke-direct {p1, p2}, Ljava/security/cert/CertificateException;-><init>(Ljava/lang/String;)V

    .line 33
    .line 34
    .line 35
    throw p1

    .line 36
    :cond_2
    :goto_0
    return-void
.end method

.method public getAcceptedIssuers()[Ljava/security/cert/X509Certificate;
    .locals 1

    .line 1
    const/4 v0, 0x0

    .line 2
    new-array v0, v0, [Ljava/security/cert/X509Certificate;

    .line 3
    .line 4
    return-object v0
.end method

## 9) MQTT CA certificate details
subject=C=CN, ST=Guangdong, L=Shenzhen, O=Anycubic, OU=Anycubic, CN=AC Root CA, emailAddress=anycubic_cloud@anycubic.com
issuer=C=CN, ST=Guangdong, L=Shenzhen, O=Anycubic, OU=Anycubic, CN=AC Root CA, emailAddress=anycubic_cloud@anycubic.com
notBefore=Jul 12 06:48:05 2023 GMT
notAfter=Jul 13 06:48:05 2123 GMT
sha256 Fingerprint=5E:AD:4D:4D:97:A7:B9:2A:13:90:64:CD:54:DC:9D:91:F3:10:36:94:55:D5:6F:70:BF:62:34:FE:39:B3:F3:98

## 10) Topic parsing helpers
.class public final Lcom/cloud/mqttservice/util/TopicUtils;
.super Ljava/lang/Object;
.source "TopicUtils.kt"


# static fields
.field public static final INSTANCE:Lcom/cloud/mqttservice/util/TopicUtils;


# direct methods
.method static constructor <clinit>()V
    .locals 1

    .line 1
    new-instance v0, Lcom/cloud/mqttservice/util/TopicUtils;

    .line 2
    .line 3
    invoke-direct {v0}, Lcom/cloud/mqttservice/util/TopicUtils;-><init>()V

    .line 4
    .line 5
    .line 6
    sput-object v0, Lcom/cloud/mqttservice/util/TopicUtils;->INSTANCE:Lcom/cloud/mqttservice/util/TopicUtils;

    .line 7
    .line 8
    return-void
.end method

.method private constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
.end method


# virtual methods
.method public final getCommandType(Ljava/lang/String;)Ljava/lang/String;
    .locals 7

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "/"

    .line 13
    .line 14
    filled-new-array {v0}, [Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v2

    .line 18
    const/4 v5, 0x6

    .line 19
    const/4 v6, 0x0

    .line 20
    const/4 v3, 0x0

    .line 21
    const/4 v4, 0x0

    .line 22
    move-object v1, p1

    .line 23
    invoke-static/range {v1 .. v6}, Lkotlin/text/q;->t0(Ljava/lang/CharSequence;[Ljava/lang/String;ZIILjava/lang/Object;)Ljava/util/List;

    .line 24
    .line 25
    .line 26
    move-result-object p1

    .line 27
    const/4 v0, 0x7

    .line 28
    invoke-interface {p1, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 29
    .line 30
    .line 31
    move-result-object p1

    .line 32
    check-cast p1, Ljava/lang/String;

    .line 33
    .line 34
    return-object p1

    .line 35
    :cond_0
    const-string p1, ""

    .line 36
    .line 37
    return-object p1
.end method

.method public final getDeviceId(Ljava/lang/String;)Ljava/lang/String;
    .locals 7

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "/"

    .line 13
    .line 14
    filled-new-array {v0}, [Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v2

    .line 18
    const/4 v5, 0x6

    .line 19
    const/4 v6, 0x0

    .line 20
    const/4 v3, 0x0

    .line 21
    const/4 v4, 0x0

    .line 22
    move-object v1, p1

    .line 23
    invoke-static/range {v1 .. v6}, Lkotlin/text/q;->t0(Ljava/lang/CharSequence;[Ljava/lang/String;ZIILjava/lang/Object;)Ljava/util/List;

    .line 24
    .line 25
    .line 26
    move-result-object p1

    .line 27
    const/4 v0, 0x6

    .line 28
    invoke-interface {p1, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 29
    .line 30
    .line 31
    move-result-object p1

    .line 32
    check-cast p1, Ljava/lang/String;

    .line 33
    .line 34
    return-object p1

    .line 35
    :cond_0
    const-string p1, ""

    .line 36
    .line 37
    return-object p1
.end method

.method public final getDeviceType(Ljava/lang/String;)Ljava/lang/String;
    .locals 7

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "/"

    .line 13
    .line 14
    filled-new-array {v0}, [Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v2

    .line 18
    const/4 v5, 0x6

    .line 19
    const/4 v6, 0x0

    .line 20
    const/4 v3, 0x0

    .line 21
    const/4 v4, 0x0

    .line 22
    move-object v1, p1

    .line 23
    invoke-static/range {v1 .. v6}, Lkotlin/text/q;->t0(Ljava/lang/CharSequence;[Ljava/lang/String;ZIILjava/lang/Object;)Ljava/util/List;

    .line 24
    .line 25
    .line 26
    move-result-object p1

    .line 27
    const/4 v0, 0x5

    .line 28
    invoke-interface {p1, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 29
    .line 30
    .line 31
    move-result-object p1

    .line 32
    check-cast p1, Ljava/lang/String;

    .line 33
    .line 34
    return-object p1

    .line 35
    :cond_0
    const-string p1, ""

    .line 36
    .line 37
    return-object p1
.end method

.method public final getDst(Ljava/lang/String;)Ljava/lang/String;
    .locals 7

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "/"

    .line 13
    .line 14
    filled-new-array {v0}, [Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v2

    .line 18
    const/4 v5, 0x6

    .line 19
    const/4 v6, 0x0

    .line 20
    const/4 v3, 0x0

    .line 21
    const/4 v4, 0x0

    .line 22
    move-object v1, p1

    .line 23
    invoke-static/range {v1 .. v6}, Lkotlin/text/q;->t0(Ljava/lang/CharSequence;[Ljava/lang/String;ZIILjava/lang/Object;)Ljava/util/List;

    .line 24
    .line 25
    .line 26
    move-result-object p1

    .line 27
    const/4 v0, 0x4

    .line 28
    invoke-interface {p1, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 29
    .line 30
    .line 31
    move-result-object p1

    .line 32
    check-cast p1, Ljava/lang/String;

    .line 33
    .line 34
    return-object p1

    .line 35
    :cond_0
    const-string p1, ""

    .line 36
    .line 37
    return-object p1
.end method

.method public final getProjectName(Ljava/lang/String;)Ljava/lang/String;
    .locals 7

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "/"

    .line 13
    .line 14
    filled-new-array {v0}, [Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v2

    .line 18
    const/4 v5, 0x6

    .line 19
    const/4 v6, 0x0

    .line 20
    const/4 v3, 0x0

    .line 21
    const/4 v4, 0x0

    .line 22
    move-object v1, p1

    .line 23
    invoke-static/range {v1 .. v6}, Lkotlin/text/q;->t0(Ljava/lang/CharSequence;[Ljava/lang/String;ZIILjava/lang/Object;)Ljava/util/List;

    .line 24
    .line 25
    .line 26
    move-result-object p1

    .line 27
    const/4 v0, 0x1

    .line 28
    invoke-interface {p1, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 29
    .line 30
    .line 31
    move-result-object p1

    .line 32
    check-cast p1, Ljava/lang/String;

    .line 33
    .line 34
    return-object p1

    .line 35
    :cond_0
    const-string p1, ""

    .line 36
    .line 37
    return-object p1
.end method

.method public final getSrc(Ljava/lang/String;)Ljava/lang/String;
    .locals 7

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "/"

    .line 13
    .line 14
    filled-new-array {v0}, [Ljava/lang/String;

    .line 15
    .line 16
    .line 17
    move-result-object v2

    .line 18
    const/4 v5, 0x6

    .line 19
    const/4 v6, 0x0

    .line 20
    const/4 v3, 0x0

    .line 21
    const/4 v4, 0x0

    .line 22
    move-object v1, p1

    .line 23
    invoke-static/range {v1 .. v6}, Lkotlin/text/q;->t0(Ljava/lang/CharSequence;[Ljava/lang/String;ZIILjava/lang/Object;)Ljava/util/List;

    .line 24
    .line 25
    .line 26
    move-result-object p1

    .line 27
    const/4 v0, 0x3

    .line 28
    invoke-interface {p1, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 29
    .line 30
    .line 31
    move-result-object p1

    .line 32
    check-cast p1, Ljava/lang/String;

    .line 33
    .line 34
    return-object p1

    .line 35
    :cond_0
    const-string p1, ""

    .line 36
    .line 37
    return-object p1
.end method

.method public final isReport(Ljava/lang/String;)Z
    .locals 2

    .line 1
    const-string v0, "topic"

    .line 2
    .line 3
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->f(Ljava/lang/Object;Ljava/lang/String;)V

    .line 4
    .line 5
    .line 6
    invoke-interface {p1}, Ljava/lang/CharSequence;->length()I

    .line 7
    .line 8
    .line 9
    move-result v0

    .line 10
    if-lez v0, :cond_0

    .line 11
    .line 12
    const-string v0, "report"

    .line 13
    .line 14
    const/4 v1, 0x1

    .line 15
    invoke-static {p1, v0, v1}, Lkotlin/text/p;->q(Ljava/lang/String;Ljava/lang/String;Z)Z

    .line 16
    .line 17
    .line 18
    move-result p1

    .line 19
    return p1

    .line 20
    :cond_0
    const/4 p1, 0x0

    .line 21
    return p1
.end method

## 11) Message dispatch + auto-response publish (/response, QoS1)

    .line 131
    .line 132
    .line 133
    new-instance v3, Ljava/lang/String;

    .line 134
    .line 135
    invoke-direct {v3, v1, v2}, Ljava/lang/String;-><init>([BLjava/nio/charset/Charset;)V

    .line 136
    .line 137
    .line 138
    const-class v1, Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;

    .line 139
    .line 140
    invoke-virtual {p1, v3, v1}, Lcom/google/gson/Gson;->i(Ljava/lang/String;Ljava/lang/Class;)Ljava/lang/Object;

    .line 141
    .line 142
    .line 143
    move-result-object p1

    .line 144
    check-cast p1, Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;

    .line 145
    .line 146
    new-instance v1, Ljava/lang/StringBuilder;

    .line 147
    .line 148
    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    .line 149
    .line 150
    .line 151
    sget-object v2, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig;->Companion:Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;

    .line 152
    .line 153
    invoke-virtual {v2}, Lcom/cloud/mqttservice/MqttServiceConstants$MQTTConfig$Companion;->getTOPIC_PUBLISH()Ljava/lang/String;

    .line 154
    .line 155
    .line 156
    move-result-object v2

    .line 157
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 158
    .line 159
    .line 160
    sget-object v2, Lcom/cloud/mqttservice/util/TopicUtils;->INSTANCE:Lcom/cloud/mqttservice/util/TopicUtils;

    .line 161
    .line 162
    invoke-virtual {v2, v0}, Lcom/cloud/mqttservice/util/TopicUtils;->getSrc(Ljava/lang/String;)Ljava/lang/String;

    .line 163
    .line 164
    .line 165
    move-result-object v3

    .line 166
    invoke-virtual {v1, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 167
    .line 168
    .line 169
    const/16 v3, 0x2f

    .line 170
    .line 171
    invoke-virtual {v1, v3}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 172
    .line 173
    .line 174
    invoke-virtual {v2, v0}, Lcom/cloud/mqttservice/util/TopicUtils;->getDeviceType(Ljava/lang/String;)Ljava/lang/String;

    .line 175
    .line 176
    .line 177
    move-result-object v4

    .line 178
    invoke-virtual {v1, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 179
    .line 180
    .line 181
    invoke-virtual {v1, v3}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 182
    .line 183
    .line 184
    invoke-virtual {v2, v0}, Lcom/cloud/mqttservice/util/TopicUtils;->getDeviceId(Ljava/lang/String;)Ljava/lang/String;

    .line 185
    .line 186
    .line 187
    move-result-object v2

    .line 188
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 189
    .line 190
    .line 191
    const-string v2, "/response"

    .line 192
    .line 193
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 194
    .line 195
    .line 196
    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 197
    .line 198
    .line 199
    move-result-object v4

    .line 200
    new-instance v1, Ljava/lang/StringBuilder;

    .line 201
    .line 202
    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    .line 203
    .line 204
    .line 205
    const-string v2, "topic "

    .line 206
    .line 207
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 208
    .line 209
    .line 210
    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 211
    .line 212
    .line 213
    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 214
    .line 215
    .line 216
    move-result-object v0

    .line 217
    invoke-static {v0}, Lcom/cloud/mqttservice/internal/ActionListenerKt;->logd(Ljava/lang/String;)V

    .line 218
    .line 219
    .line 220
    new-instance v0, Ljava/lang/StringBuilder;

    .line 221
    .line 222
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    .line 223
    .line 224
    .line 225
    const-string v1, "publishTopic "

    .line 226
    .line 227
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 228
    .line 229
    .line 230
    invoke-virtual {v0, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 231
    .line 232
    .line 233
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    .line 234
    .line 235
    .line 236
    move-result-object v0

    .line 237
    invoke-static {v0}, Lcom/cloud/mqttservice/internal/ActionListenerKt;->logd(Ljava/lang/String;)V

    .line 238
    .line 239
    .line 240
    new-instance v0, Lac/cloud/publicbean/main/data/mqtt/PublishMqttMsg;

    .line 241
    .line 242
    invoke-virtual {p1}, Lac/cloud/publicbean/main/data/mqtt/BaseReceivedMqttMsgBean;->getMsgid()Ljava/lang/String;

    .line 243
    .line 244
    .line 245
    move-result-object p1

    .line 246
    invoke-direct {v0, p1}, Lac/cloud/publicbean/main/data/mqtt/PublishMqttMsg;-><init>(Ljava/lang/String;)V

    .line 247
    .line 248
    .line 249
    invoke-static {}, Lcom/cloud/internet/factory/data/c;->b()Lcom/google/gson/Gson;

    .line 250
    .line 251
    .line 252
    move-result-object p1

    .line 253
    invoke-virtual {p1, v0}, Lcom/google/gson/Gson;->r(Ljava/lang/Object;)Ljava/lang/String;

    .line 254
    .line 255
    .line 256
    move-result-object p1

    .line 257
    const-string v0, "toJson(...)"

    .line 258
    .line 259
    invoke-static {p1, v0}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 260
    .line 261
    .line 262
    sget-object v0, Lkotlin/text/c;->b:Ljava/nio/charset/Charset;

    .line 263
    .line 264
    invoke-virtual {p1, v0}, Ljava/lang/String;->getBytes(Ljava/nio/charset/Charset;)[B

    .line 265
    .line 266
    .line 267
    move-result-object v5

    .line 268
    const-string p1, "getBytes(...)"

    .line 269
    .line 270
    invoke-static {v5, p1}, Lkotlin/jvm/internal/m;->e(Ljava/lang/Object;Ljava/lang/String;)V

    .line 271
    .line 272
    .line 273
    sget-object p1, Lcom/cloud/mqttservice/QoS;->AtLeastOnce:Lcom/cloud/mqttservice/QoS;

    .line 274
    .line 275
    invoke-virtual {p1}, Lcom/cloud/mqttservice/QoS;->getValue()I

    .line 276
    .line 277
    .line 278
    move-result v6

    .line 279
    const/4 v8, 0x0

    .line 280
    const/4 v9, 0x0

    .line 281
    const/4 v7, 0x0

    .line 282
    move-object v3, p0

    .line 283
    invoke-virtual/range {v3 .. v9}, Lcom/cloud/mqttservice/internal/Connection;->publish(Ljava/lang/String;[BIZLjava/lang/Object;Ltb/c;)V

    .line 284
    .line 285
    .line 286
    return-void
.end method

.method private final notifyListeners(Ljava/beans/PropertyChangeEvent;)V
    .locals 2

    .line 1
    iget-object v0, p0, Lcom/cloud/mqttservice/internal/Connection;->listeners:Ljava/util/ArrayList;

    .line 2
    .line 3
    invoke-interface {v0}, Ljava/lang/Iterable;->iterator()Ljava/util/Iterator;

    .line 4
    .line 5
    .line 6
    move-result-object v0

    .line 7
    :goto_0
    invoke-interface {v0}, Ljava/util/Iterator;->hasNext()Z

    .line 8
    .line 9
    .line 10
    move-result v1

    .line 11

## 12) Command/action constants
.class public final Lac/cloud/publicbean/main/data/mqtt/MQTTConstants;
.super Ljava/lang/Object;
.source "MQTTConstants.kt"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Lac/cloud/publicbean/main/data/mqtt/MQTTConstants$Companion;
    }
.end annotation


# static fields
.field public static final COMMAND_ID_CANCEL_EXPOSURE:I = 0x12e

.field public static final COMMAND_ID_CANCEL_RESIDUAL:I = 0x1f6

.field public static final COMMAND_ID_CHECK_LIGHT_STATUS:I = 0x4d0

.field public static final COMMAND_ID_CYCLIC_CLEANING:I = 0x4ca

.field public static final COMMAND_ID_DETECT:I = 0xc

.field public static final COMMAND_ID_EXTFILBOX:I = 0x4cd

.field public static final COMMAND_ID_FEED_RESIN:I = 0x4c8

.field public static final COMMAND_ID_FILAMNET_CONTROL:I = 0x4bf

.field public static final COMMAND_ID_FINISH_FEED_FILAMENT:I = 0x4b9

.field public static final COMMAND_ID_GET_AUTO_OPERATION:I = 0x2be

.field public static final COMMAND_ID_GET_DEVICE_SELF_TEST:I = 0x25a

.field public static final COMMAND_ID_GET_EXTFILBOX_INFO:I = 0x4ce

.field public static final COMMAND_ID_GET_M7_AUTO_OPERATION:I = 0x4cc

.field public static final COMMAND_ID_GET_MULTI_COLOR_BOX_INFO:I = 0x4b6

.field public static final COMMAND_ID_GET_RELEASE_FILM:I = 0x322

.field public static final COMMAND_ID_IGNORE:I = 0xb

.field public static final COMMAND_ID_INFO_REPORT:I = 0x4dc

.field public static final COMMAND_ID_LOCAL:I = 0x67

.field public static final COMMAND_ID_LOCAL_DELETE:I = 0x68

.field public static final COMMAND_ID_LOCAL_FILE_BATCH_DELETE:I = 0x4d8

.field public static final COMMAND_ID_LOCAL_FILE_INFO:I = 0x4d7

.field public static final COMMAND_ID_M7_AUTO_OPERATION:I = 0x4c9

.field public static final COMMAND_ID_MOVE:I = 0xc9

.field public static final COMMAND_ID_MOVE_TO_COORDINATES:I = 0xca

.field public static final COMMAND_ID_PAUSE:I = 0x2

.field public static final COMMAND_ID_QUERY_AI_SETTINGS:I = 0x4da

.field public static final COMMAND_ID_QUERY_PERIPHERAL:I = 0x4cf

.field public static final COMMAND_ID_REFRESH_SLOT:I = 0x4ba

.field public static final COMMAND_ID_RESET_RELEASE_FILM:I = 0x321

.field public static final COMMAND_ID_RESUME:I = 0x3

.field public static final COMMAND_ID_SETTING:I = 0x6

.field public static final COMMAND_ID_SET_AI_SETTINGS:I = 0x4db

.field public static final COMMAND_ID_SET_AUTO_FEED:I = 0x4bc

.field public static final COMMAND_ID_SET_AUTO_FEED_INFO:I = 0x4cb

.field public static final COMMAND_ID_SET_AUTO_OPERATION:I = 0x2bd

.field public static final COMMAND_ID_SET_DEVICE_SELF_TEST:I = 0x259

.field public static final COMMAND_ID_SET_DRY:I = 0x4b7

.field public static final COMMAND_ID_SET_FEED_FILAMENT:I = 0x4b8

.field public static final COMMAND_ID_SET_LIGHT_STATUS:I = 0x4d1

.field public static final COMMAND_ID_SET_PRINT_STATUS_FREE:I = 0x385

.field public static final COMMAND_ID_SET_SLOT_INFO:I = 0x4bb

.field public static final COMMAND_ID_SKIPPED_PART:I = 0x4d4

.field public static final COMMAND_ID_SKIP_PART:I = 0x4d6

.field public static final COMMAND_ID_START:I = 0x1

.field public static final COMMAND_ID_START_EXPOSURE:I = 0x12d

.field public static final COMMAND_ID_START_RESIDUAL:I = 0x1f5

.field public static final COMMAND_ID_STATUS:I = 0x5

.field public static final COMMAND_ID_STOP:I = 0x4

.field public static final COMMAND_ID_STOP_FORCE:I = 0x2c

.field public static final COMMAND_ID_UNBIND_PRINTER:I = 0x4dd

.field public static final COMMAND_ID_U_DISK:I = 0x65

.field public static final COMMAND_ID_U_DISK_DELETE:I = 0x66

.field public static final Companion:Lac/cloud/publicbean/main/data/mqtt/MQTTConstants$Companion;

.field public static final MQTT_ACTION_AUTO:Ljava/lang/String; = "auto"

.field public static final MQTT_ACTION_AUTO_OPERATION:Ljava/lang/String; = "autoOperation"

.field public static final MQTT_ACTION_AUTO_UPDATE_DRY_STATUS:Ljava/lang/String; = "autoUpdateDryStatus"

.field public static final MQTT_ACTION_AUTO_UPDATE_INFO:Ljava/lang/String; = "autoUpdateInfo"

.field public static final MQTT_ACTION_CANCEL:Ljava/lang/String; = "cancel"

.field public static final MQTT_ACTION_CLEAN:Ljava/lang/String; = "clean"

.field public static final MQTT_ACTION_CONTROL:Ljava/lang/String; = "control"

.field public static final MQTT_ACTION_CYCLIC_CLEANING:Ljava/lang/String; = "cyclicCleaning"

.field public static final MQTT_ACTION_DELETE_BATCH:Ljava/lang/String; = "deleteBatch"

.field public static final MQTT_ACTION_DELETE_LOCAL:Ljava/lang/String; = "deleteLocal"

.field public static final MQTT_ACTION_DELETE_U_DISK:Ljava/lang/String; = "deleteUdisk"

.field public static final MQTT_ACTION_FEED_FILAMENT:Ljava/lang/String; = "feedFilament"

.field public static final MQTT_ACTION_FEED_RESIN:Ljava/lang/String; = "feedResin"

.field public static final MQTT_ACTION_FILE_DETAILS:Ljava/lang/String; = "fileDetails"

.field public static final MQTT_ACTION_GET:Ljava/lang/String; = "get"

.field public static final MQTT_ACTION_GET_INFO:Ljava/lang/String; = "getInfo"

.field public static final MQTT_ACTION_IGNORE:Ljava/lang/String; = "ignore"

.field public static final MQTT_ACTION_LOCAL:Ljava/lang/String; = "listLocal"

.field public static final MQTT_ACTION_M7_REPORT_STATUS:Ljava/lang/String; = "reportStatus"

.field public static final MQTT_ACTION_MONITOR:Ljava/lang/String; = "monitor"

.field public static final MQTT_ACTION_MOVE:Ljava/lang/String; = "move"

.field public static final MQTT_ACTION_NET:Ljava/lang/String; = "net"

.field public static final MQTT_ACTION_ONLINE_REPORT:Ljava/lang/String; = "onlineReport"

.field public static final MQTT_ACTION_PAUSE:Ljava/lang/String; = "pause"

.field public static final MQTT_ACTION_QUERY:Ljava/lang/String; = "query"

.field public static final MQTT_ACTION_QUERY_OBJ:Ljava/lang/String; = "query_obj"

.field public static final MQTT_ACTION_REDETECT:Ljava/lang/String; = "reDetect"

.field public static final MQTT_ACTION_REFRESH:Ljava/lang/String; = "refresh"

.field public static final MQTT_ACTION_REPLACE:Ljava/lang/String; = "replace"

.field public static final MQTT_ACTION_REPORT:Ljava/lang/String; = "report"

.field public static final MQTT_ACTION_REPORT_INFO:Ljava/lang/String; = "reportInfo"

.field public static final MQTT_ACTION_RESET:Ljava/lang/String; = "reset"

.field public static final MQTT_ACTION_RESUME:Ljava/lang/String; = "resume"

.field public static final MQTT_ACTION_SET:Ljava/lang/String; = "set"

.field public static final MQTT_ACTION_SET_AUTO_FEED:Ljava/lang/String; = "setAutoFeed"

.field public static final MQTT_ACTION_SET_AUTO_FEED_INFO:Ljava/lang/String; = "reportAutoFeedInfo"

.field public static final MQTT_ACTION_SET_DRY:Ljava/lang/String; = "setDry"

.field public static final MQTT_ACTION_SET_INFO:Ljava/lang/String; = "setInfo"

.field public static final MQTT_ACTION_START:Ljava/lang/String; = "start"

.field public static final MQTT_ACTION_START_CAPTURE:Ljava/lang/String; = "startCapture"

.field public static final MQTT_ACTION_STATUS:Ljava/lang/String; = "status"

.field public static final MQTT_ACTION_STOP:Ljava/lang/String; = "stop"

.field public static final MQTT_ACTION_STOP_CAPTURE:Ljava/lang/String; = "stopCapture"

.field public static final MQTT_ACTION_TEMP_REPORT_INFO:Ljava/lang/String; = "reportInfo"

.field public static final MQTT_ACTION_TIP:Ljava/lang/String; = "tip"

.field public static final MQTT_ACTION_TURN_OFF:Ljava/lang/String; = "turnOff"

.field public static final MQTT_ACTION_UNBIND:Ljava/lang/String; = "unbind"

.field public static final MQTT_ACTION_UPDATE:Ljava/lang/String; = "update"

.field public static final MQTT_ACTION_U_DISK:Ljava/lang/String; = "listUdisk"

.field public static final MQTT_ACTION_WORK_REPORT:Ljava/lang/String; = "workReport"

.field public static final MQTT_FDM_BUSY_10301:I = 0x283d

.field public static final MQTT_FDM_BUSY_10302:I = 0x283e

.field public static final MQTT_FDM_BUSY_10303:I = 0x283f

.field public static final MQTT_FDM_FAILED_10101:I = 0x2775

.field public static final MQTT_FDM_FAILED_10102:I = 0x2776

.field public static final MQTT_FDM_FAILED_10103:I = 0x2777

.field public static final MQTT_FDM_FAILED_10104:I = 0x2778

.field public static final MQTT_FDM_FAILED_10105:I = 0x2779

.field public static final MQTT_FDM_FAILED_10106:I = 0x277a

.field public static final MQTT_FDM_FAILED_10110:I = 0x277e

.field public static final MQTT_FDM_FAILED_10111:I = 0x277f

.field public static final MQTT_FDM_FAILED_10112:I = 0x2780

.field public static final MQTT_FDM_FAILED_10113:I = 0x2781

.field public static final MQTT_FDM_FAILED_10115:I = 0x2783

.field public static final MQTT_FDM_FAILED_10117:I = 0x2785

.field public static final MQTT_FDM_FAILED_10118:I = 0x2786

.field public static final MQTT_FDM_FAILED_10119:I = 0x2787

.field public static final MQTT_FDM_FAILED_10120:I = 0x2788

.field public static final MQTT_FDM_FAILED_10121:I = 0x2789

.field public static final MQTT_FDM_FAILED_10122:I = 0x278a

.field public static final MQTT_FDM_FAILED_10123:I = 0x278b

.field public static final MQTT_FDM_FAILED_10124:I = 0x278c

.field public static final MQTT_FDM_FAILED_10125:I = 0x278d

.field public static final MQTT_FDM_PAUSE_10401:I = 0x28a1

.field public static final MQTT_FDM_PAUSE_10402:I = 0x28a2

.field public static final MQTT_FDM_RESUME_10501:I = 0x2905

.field public static final MQTT_FDM_RESUME_10502:I = 0x2906

.field public static final MQTT_FDM_STOP_10601:I = 0x2969

.field public static final MQTT_FDM_STOP_10602:I = 0x296a

.field public static final MQTT_FDM_STOP_10603:I = 0x296b

.field public static final MQTT_FILE_TYPE_LOCAL:Ljava/lang/String; = "local"

.field public static final MQTT_FILE_TYPE_U_DISK:Ljava/lang/String; = "Udisk"

.field public static final MQTT_LCD_BUSY_301:I = 0x12d

.field public static final MQTT_LCD_BUSY_302:I = 0x12e

.field public static final MQTT_LCD_BUSY_303:I = 0x12f

.field public static final MQTT_LCD_ERROR_CODE_1101:I = 0x44d

.field public static final MQTT_LCD_ERROR_CODE_1102:I = 0x44e

.field public static final MQTT_LCD_ERROR_CODE_1103:I = 0x44f

.field public static final MQTT_LCD_ERROR_CODE_1104:I = 0x450

.field public static final MQTT_LCD_ERROR_CODE_1201:I = 0x4b1

.field public static final MQTT_LCD_ERROR_CODE_1202:I = 0x4b2

.field public static final MQTT_LCD_ERROR_CODE_1203:I = 0x4b3

.field public static final MQTT_LCD_ERROR_CODE_1204:I = 0x4b4

.field public static final MQTT_LCD_ERROR_CODE_1301:I = 0x515

.field public static final MQTT_LCD_FAILED_101:I = 0x65

.field public static final MQTT_LCD_FAILED_102:I = 0x66

.field public static final MQTT_LCD_FAILED_103:I = 0x67

.field public static final MQTT_LCD_FAILED_104:I = 0x68

.field public static final MQTT_LCD_FAILED_105:I = 0x69

.field public static final MQTT_LCD_FAILED_106:I = 0x6a

.field public static final MQTT_LCD_FAILED_107:I = 0x6b

.field public static final MQTT_LCD_FAILED_108:I = 0x6c

.field public static final MQTT_LCD_FAILED_110:I = 0x6e

.field public static final MQTT_LCD_FAILED_111:I = 0x6f

.field public static final MQTT_LCD_FAILED_112:I = 0x70

.field public static final MQTT_LCD_FAILED_113:I = 0x71

.field public static final MQTT_LCD_FAILED_115:I = 0x73

.field public static final MQTT_LCD_FAILED_117:I = 0x75

.field public static final MQTT_LCD_FAILED_118:I = 0x76

.field public static final MQTT_LCD_FAILED_119:I = 0x77

.field public static final MQTT_LCD_FAILED_120:I = 0x78

.field public static final MQTT_LCD_FAILED_121:I = 0x79

.field public static final MQTT_LCD_FAILED_122:I = 0x7a

.field public static final MQTT_LCD_FAILED_123:I = 0x7b

.field public static final MQTT_LCD_PAUSE_401:I = 0x191

.field public static final MQTT_LCD_PAUSE_402:I = 0x192

.field public static final MQTT_LCD_RESUME_501:I = 0x1f5

.field public static final MQTT_LCD_RESUME_502:I = 0x1f6

.field public static final MQTT_LCD_STOP_601:I = 0x259

.field public static final MQTT_LCD_STOP_602:I = 0x25a

.field public static final MQTT_LCD_STOP_603:I = 0x25b

.field public static final MQTT_LIGHT_TYPE_BOX:I = 0x2

.field public static final MQTT_LIGHT_TYPE_CAMERA:I = 0x3

.field public static final MQTT_LIGHT_TYPE_NOZZLE:I = 0x1

.field public static final MQTT_OPERATION_PHP_SUCCESS_CODE:I = 0x0

.field public static final MQTT_OPERATION_SUCCESS_CODE:I = 0xc8

.field public static final MQTT_P2P_VIDEO_11401:I = 0x2c89

.field public static final MQTT_P2P_VIDEO_11402:I = 0x2c8a

.field public static final MQTT_P2P_VIDEO_11403:I = 0x2c8b

.field public static final MQTT_P2P_VIDEO_11404:I = 0x2c8c

.field public static final MQTT_P2P_VIDEO_11405:I = 0x2c8d

.field public static final MQTT_P2P_VIDEO_11408:I = 0x2c90

.field public static final MQTT_P2P_VIDEO_11410:I = 0x2c92

.field public static final MQTT_P2P_VIDEO_1401:I = 0x579

.field public static final MQTT_P2P_VIDEO_1402:I = 0x57a

.field public static final MQTT_P2P_VIDEO_1403:I = 0x57b

.field public static final MQTT_P2P_VIDEO_1404:I = 0x57c

.field public static final MQTT_P2P_VIDEO_1405:I = 0x57d

.field public static final MQTT_P2P_VIDEO_200:I = 0xc8

.field public static final MQTT_PRINTER_STATUS_BUSY:Ljava/lang/String; = "busy"

.field public static final MQTT_PRINTER_STATUS_FREE:Ljava/lang/String; = "free"

.field public static final MQTT_PRINTER_STATUS_IN_QUEUE:Ljava/lang/String; = "inQueue"

.field public static final MQTT_PRINTER_STATUS_OFFLINE:Ljava/lang/String; = "offline"

.field public static final MQTT_PRINTER_STATUS_ONLINE:Ljava/lang/String; = "online"

.field public static final MQTT_PRINTER_STATUS_SLICE_CANCEL:Ljava/lang/String; = "sliceCancel"

.field public static final MQTT_PRINTER_STATUS_SLICE_FAILED:Ljava/lang/String; = "sliceFailed"

.field public static final MQTT_PRINTER_STATUS_SLICE_SUCCESS:Ljava/lang/String; = "sliceSuccess"

.field public static final MQTT_PRINTER_STATUS_SLICE_SUCCESS_PROCESSED:Ljava/lang/String; = "sliceSuccessProcessed"

.field public static final MQTT_PRINTER_STATUS_SLICING:Ljava/lang/String; = "slicing"

.field public static final MQTT_PRINTER_STATUS_UPDATE_QUEUE:Ljava/lang/String; = "updateQueue"


## 13) Result listener callbacks (app-level MQTT events)
.class public interface abstract Lac/cloud/common/ext/r;
.super Ljava/lang/Object;
.source "MqttResultListener.kt"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Lac/cloud/common/ext/r$a;
    }
.end annotation


# virtual methods
.method public abstract onMqttBatchDelete(Lac/cloud/publicbean/main/data/mqtt/BaseReceivedMqttMsgBean;)V
.end method

.method public abstract onMqttInfoReport(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterInfoReportBean;)V
.end method

.method public abstract onMqttMultiColorBoxOTAEvent(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterOTABean;Ljava/lang/Integer;)V
.end method

.method public abstract onMqttPrintAutoOperationEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintAutoOperationGetLcdEvent(Lac/cloud/publicbean/main/data/mqtt/SelfTestData;)V
.end method

.method public abstract onMqttPrintAutoOperationSetLcdEvent(Lac/cloud/publicbean/main/data/mqtt/SelfTestData;)V
.end method

.method public abstract onMqttPrintAxisEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintBoxSlotLoadedUpdate(Lac/cloud/publicbean/main/data/response/SlotLoadedUpdateResponse;)V
.end method

.method public abstract onMqttPrintCyclicCleaningEvent(Lac/cloud/publicbean/main/data/SmartResinVatRemainTimeInfo;)V
.end method

.method public abstract onMqttPrintDeleteLocalFileEvent(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterFileBean;)V
.end method

.method public abstract onMqttPrintDeleteUDiskFileEvent(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterFileBean;)V
.end method

.method public abstract onMqttPrintEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintExposureCancelEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintExposureStartEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintExternalRackEditSlotEvent(Lac/cloud/publicbean/main/data/response/ExternalRackSlotInfoResponse;)V
.end method

.method public abstract onMqttPrintExternalRackFilamentControlEvent(Lac/cloud/publicbean/main/data/response/FilamentControlResponse;)V
.end method

.method public abstract onMqttPrintFanEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintFeedResinEvent(Lac/cloud/publicbean/main/data/mqtt/FeedResin;)V
.end method

.method public abstract onMqttPrintLocalFileEvent(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterFileBean;)V
.end method

.method public abstract onMqttPrintM7proAutoOperationEvent(Lac/cloud/publicbean/main/data/mqtt/CurrentStatusData;)V
.end method

.method public abstract onMqttPrintMonitorEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintMultiColorBoxDryEvent(Lac/cloud/publicbean/main/data/response/DryStatusResponse;)V
.end method

.method public abstract onMqttPrintMultiColorBoxFeedFilamentEvent(Lac/cloud/publicbean/main/data/response/FeedFilamentResponse;)V
.end method

.method public abstract onMqttPrintMultiColorBoxGetInfoEvent(Lac/cloud/publicbean/main/data/response/MultiBoxInfoResponse;)V
.end method

.method public abstract onMqttPrintMultiColorBoxRefreshEvent(Lac/cloud/publicbean/main/data/response/SlotInfoResponse;)V
.end method

.method public abstract onMqttPrintMultiColorBoxSetAutoFeedEvent(Lac/cloud/publicbean/main/data/response/FeedFilamentResponse;)V
.end method

.method public abstract onMqttPrintMultiColorBoxSetInfoEvent(Lac/cloud/publicbean/main/data/response/SlotInfoResponse;)V
.end method

.method public abstract onMqttPrintOatEvent(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterOTABean;)V
.end method

.method public abstract onMqttPrintPeripheralState(Lac/cloud/publicbean/main/data/response/PeripheralStateResponse;)V
.end method

.method public abstract onMqttPrintReleaseFilmGetEvent(Lac/cloud/publicbean/main/data/ReleaseFilmBean;)V
.end method

.method public abstract onMqttPrintReleaseFilmResetEvent(Lac/cloud/publicbean/main/data/ReleaseFilmBean;)V
.end method

.method public abstract onMqttPrintResidualCancelEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintResidualCleanEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintSliceEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintStatusEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintTemperatureAutoEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintTemperatureEvent(Lac/cloud/publicbean/main/data/mqtt/PrinterMqttPrintingBean;)V
.end method

.method public abstract onMqttPrintTurnOffEvent(Lac/cloud/publicbean/main/data/mqtt/ReceivedMqttMsgBeanNoData;)V
.end method

.method public abstract onMqttPrintUDiskFileEvent(Lac/cloud/publicbean/main/data/mqtt/MqttPrinterFileBean;)V
.end method

.method public abstract onMqttPrintVideoCaptureEvent(Lac/cloud/publicbean/main/data/response/PeerCredentials;)V
.end method

.method public abstract onMqttPrinterLightState(Lac/cloud/publicbean/main/data/response/PrinterLightMqttResponse;)V
.end method

.method public abstract onMqttPrinterLightStateChange(Lac/cloud/publicbean/main/data/response/PrinterLightChangeMqttResponse;)V
.end method

.method public abstract onMqttPrinterLocalFileInfo(Lac/cloud/publicbean/main/data/response/PrinterLocalFileInfoMqttResponse;)V
.end method

.method public abstract onMqttQueryAISettings(Lac/cloud/publicbean/main/data/mqtt/AISettingsMqttReceiveInfo;)V
.end method

.method public abstract onMqttQuerySkipPart(Lac/cloud/publicbean/main/data/mqtt/SkipPartMqttReceiveInfo;)V
.end method

.method public abstract onMqttSetAISettings(Lac/cloud/publicbean/main/data/mqtt/AISettingsMqttReceiveInfo;)V
.end method

.method public abstract onMqttUnbindPrinter(Lac/cloud/publicbean/main/data/mqtt/UnbindPrinterMqttReceiveInfo;)V
.end method

## 14) ARouter mappings for mqttservice
.class public Lcom/alibaba/android/arouter/routes/ARouter$$Group$$mqttservice;
.super Ljava/lang/Object;
.source "ARouter$$Group$$mqttservice.java"

# interfaces
.implements Lcom/alibaba/android/arouter/facade/template/IRouteGroup;


# direct methods
.method public constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
.end method


# virtual methods
.method public loadInto(Ljava/util/Map;)V
    .locals 7
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Ljava/util/Map<",
            "Ljava/lang/String;",
            "Lcom/alibaba/android/arouter/facade/model/RouteMeta;",
            ">;)V"
        }
    .end annotation

    .line 1
    sget-object v0, Lcom/alibaba/android/arouter/facade/enums/RouteType;->PROVIDER:Lcom/alibaba/android/arouter/facade/enums/RouteType;

    .line 2
    .line 3
    const/4 v5, -0x1

    .line 4
    const/high16 v6, -0x80000000

    .line 5
    .line 6
    const-class v1, Lcom/cloud/mqttservice/MqttModelServiceImpl;

    .line 7
    .line 8
    const-string v2, "/mqttservice/mqttmodelserviceimpl"

    .line 9
    .line 10
    const-string v3, "mqttservice"

    .line 11
    .line 12
    const/4 v4, 0x0

    .line 13
    invoke-static/range {v0 .. v6}, Lcom/alibaba/android/arouter/facade/model/RouteMeta;->build(Lcom/alibaba/android/arouter/facade/enums/RouteType;Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;II)Lcom/alibaba/android/arouter/facade/model/RouteMeta;

    .line 14
    .line 15
    .line 16
    move-result-object v0

    .line 17
    const-string v1, "/mqttservice/MqttModelServiceImpl"

    .line 18
    .line 19
    invoke-interface {p1, v1, v0}, Ljava/util/Map;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;

    .line 20
    .line 21
    .line 22
    return-void
.end method

.class public Lcom/alibaba/android/arouter/routes/ARouter$$Providers$$mqttservice;
.super Ljava/lang/Object;
.source "ARouter$$Providers$$mqttservice.java"

# interfaces
.implements Lcom/alibaba/android/arouter/facade/template/IProviderGroup;


# direct methods
.method public constructor <init>()V
    .locals 0

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
.end method


# virtual methods
.method public loadInto(Ljava/util/Map;)V
    .locals 7
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Ljava/util/Map<",
            "Ljava/lang/String;",
            "Lcom/alibaba/android/arouter/facade/model/RouteMeta;",
            ">;)V"
        }
    .end annotation

    .line 1
    sget-object v0, Lcom/alibaba/android/arouter/facade/enums/RouteType;->PROVIDER:Lcom/alibaba/android/arouter/facade/enums/RouteType;

    .line 2
    .line 3
    const/4 v5, -0x1

    .line 4
    const/high16 v6, -0x80000000

    .line 5
    .line 6
    const-class v1, Lcom/cloud/mqttservice/MqttModelServiceImpl;

    .line 7
    .line 8
    const-string v2, "/mqttservice/MqttModelServiceImpl"

    .line 9
    .line 10
    const-string v3, "mqttservice"

    .line 11
    .line 12
    const/4 v4, 0x0

    .line 13
    invoke-static/range {v0 .. v6}, Lcom/alibaba/android/arouter/facade/model/RouteMeta;->build(Lcom/alibaba/android/arouter/facade/enums/RouteType;Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;II)Lcom/alibaba/android/arouter/facade/model/RouteMeta;

    .line 14
    .line 15
    .line 16
    move-result-object v0

    .line 17
    const-string v1, "ac.cloud.reposervice.service.IMqttModelService"

    .line 18
    .line 19
    invoke-interface {p1, v1, v0}, Ljava/util/Map;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;

    .line 20
    .line 21
    .line 22
    return-void
.end method

## 15) Eclipse Paho evidence
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3608:Name: org/eclipse/paho/client/mqttv3/internal/nls/logcat.properties
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3611:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages.properties
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3614:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_cs.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3618:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_de.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3622:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_es.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3626:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_fr.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3630:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_hu.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3634:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_it.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3638:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_ja.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3642:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_ko.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3646:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_pl.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3650:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_pt_BR.prope
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3654:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_ru.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3658:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_zh_CN.prope
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3662:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_zh_TW.prope
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3666:Name: org/eclipse/paho/client/mqttv3/logging/jsr47min.properties
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/CERT.SF:3669:Name: org/eclipse/paho/client/mqttv3/package-info.html
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3607:Name: org/eclipse/paho/client/mqttv3/internal/nls/logcat.properties
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3610:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages.properties
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3613:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_cs.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3617:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_de.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3621:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_es.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3625:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_fr.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3629:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_hu.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3633:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_it.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3637:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_ja.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3641:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_ko.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3645:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_pl.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3649:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_pt_BR.prope
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3653:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_ru.properti
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3657:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_zh_CN.prope
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3661:Name: org/eclipse/paho/client/mqttv3/internal/nls/messages_zh_TW.prope
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3665:Name: org/eclipse/paho/client/mqttv3/logging/jsr47min.properties
~/apk/Anycubic_1.1.27_apkcombo.com/original/META-INF/MANIFEST.MF:3668:Name: org/eclipse/paho/client/mqttv3/package-info.html
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/bundle.properties:14:bundle.name=Paho MQTT Client
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/logging/jsr47min.properties:41:org.eclipse.paho.mqttv5.client.handlers=java.util.logging.MemoryHandler
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/logging/jsr47min.properties:42:org.eclipse.paho.mqttv5.client.level=ALL
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/logging/jsr47min.properties:44:#org.eclipse.paho.mqttv5.client.internal.ClientComms.level=ALL
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/logging/jsr47min.properties:76:java.util.logging.FileHandler.formatter=org.eclipse.paho.client.mqttv3.logging.SimpleLogFormatter
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/logging/jsr47min.properties:82:#java.util.logging.ConsoleHandler.formatter=org.eclipse.paho.client.mqttv3.logging.SimpleLogFormatter
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:41:        <li>Create an instance of {@link org.eclipse.paho.client.mqttv3.MqttClient} or {@link org.eclipse.paho.client.mqttv3.MqttAsyncClient},
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:68:            case where a file based persistent store {@link org.eclipse.paho.client.mqttv3.persist.MqttDefaultFilePersistence
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:70:        <li>When connecting the {@link org.eclipse.paho.client.mqttv3.MqttConnectOptions#setCleanSession(boolean) cleansession}
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:93:                <li>{@link org.eclipse.paho.client.mqttv3.IMqttAsyncClient MqttAsyncClient} which provides a non-blocking interface
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:97:                        <li>Use the {@link org.eclipse.paho.client.mqttv3.IMqttToken#waitForCompletion waitForCompletion} call
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:99:                        <li>Pass a {@link org.eclipse.paho.client.mqttv3.IMqttActionListener IMqttActionListener} to the operation.
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:101:                        <li>Set a {@link org.eclipse.paho.client.mqttv3.MqttCallback MqttCallback} on the client. It will be
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:106:                <li>{@link org.eclipse.paho.client.mqttv3.IMqttClient MqttClient} where methods block until the operation has
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:112:                <li>Notification that a new message has arrived: {@link org.eclipse.paho.client.mqttv3.MqttCallback#messageArrived
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:114:                <li>Notification that the connection to the server has broken: {@link org.eclipse.paho.client.mqttv3.MqttCallback#connectionLost
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:116:                <li>Notification that a message has been delivered to the server: {@link org.eclipse.paho.client.mqttv3.MqttCallback#deliveryComplete
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:119:            A client registers interest in these notifications by registering a {@link org.eclipse.paho.client.mqttv3.MqttCallback MqttCallback}
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:123:                <li>{@link org.eclipse.paho.sample.mqttv3app.Sample} uses the blocking client interface</li>
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:124:                <li>{@link org.eclipse.paho.sample.mqttv3app.SampleAsyncCallBack} uses the asynchronous client with callbacks
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:126:                <li>{@link org.eclipse.paho.sample.mqttv3app.SampleAsyncWait} uses the asynchronous client and shows how to use
~/apk/Anycubic_1.1.27_apkcombo.com/unknown/org/eclipse/paho/client/mqttv3/package-info.html:130:        <li>{@link org.eclipse.paho.client.mqttv3.MqttConnectOptions MqttConnectOptions} can be used to override the default
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/r.smali:3:.source "MqttPingSender.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/o.smali:3:.source "MqttMessage.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/i.smali:3:.source "MqttCallback.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/l.smali:3:.source "MqttConnectOptions.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/m.smali:3:.source "MqttDeliveryToken.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/u.smali:3:.source "MqttTopic.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/p.smali:3:.source "MqttPersistable.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/h.smali:3:.source "MqttAsyncClient.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/h.smali:140:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/n.smali:3:.source "MqttException.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/k.smali:3:.source "MqttClientPersistence.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/h$a.smali:3:.source "MqttAsyncClient.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/s.smali:3:.source "MqttSecurityException.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/h$b.smali:3:.source "MqttAsyncClient.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/h$c.smali:3:.source "MqttAsyncClient.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/q.smali:3:.source "MqttPersistenceException.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/t.smali:3:.source "MqttToken.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/tb/j.smali:3:.source "MqttCallbackExtended.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/zb/b.smali:3:.source "MqttDefaultFilePersistence.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/e.smali:3:.source "MqttDisconnect.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/r.smali:3:.source "MqttSubscribe.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/o.smali:3:.source "MqttPublish.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/i.smali:3:.source "MqttPingReq.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/l.smali:3:.source "MqttPubComp.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/u.smali:3:.source "MqttWireMessage.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/p.smali:3:.source "MqttReceivedMessage.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/d.smali:3:.source "MqttConnect.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/n.smali:3:.source "MqttPubRel.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/f.smali:3:.source "MqttInputStream.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/f.smali:50:    const-string v1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/q.smali:3:.source "MqttSuback.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/j.smali:3:.source "MqttPingResp.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/m.smali:3:.source "MqttPubRec.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/h.smali:3:.source "MqttPersistableWireMessage.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/b.smali:3:.source "MqttAck.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/c.smali:3:.source "MqttConnack.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/k.smali:3:.source "MqttPubAck.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/s.smali:3:.source "MqttUnsubAck.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/g.smali:3:.source "MqttOutputStream.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/g.smali:35:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/xb/t.smali:3:.source "MqttUnsubscribe.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/wb/i.smali:45:    const-string p1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/wb/f.smali:45:    const-string p1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/wb/h.smali:53:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/vb/a.smali:987:    const-string v2, "org.eclipse.paho.client.mqttv3.internal.security.SSLSocketFactoryFactory"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/vb/a.smali:1364:    const-string v5, "org.eclipse.paho.client.mqttv3.internal.security.SSLSocketFactoryFactory"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/e.smali:69:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/r.smali:20:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.messages"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/o.smali:3:.source "MqttPersistentData.java"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/d.smali:69:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/n.smali:34:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.MIDPCatalog"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/x.smali:61:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/f.smali:41:    const-string v1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/a.smali:97:    const-string v1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/b.smali:98:    const-string v1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/q.smali:39:    const-string v2, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/c.smali:106:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/v.smali:44:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/h.smali:59:    const-string v0, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes8/ub/s.smali:43:    const-string p1, "org.eclipse.paho.client.mqttv3.internal.nls.logcat"
