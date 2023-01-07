# Android DeepLink

## Brief introduction

Android DeepLink means that web developer could craft a link, and when user click that link in chromium-like browser, it will redirect to the app, or if you haven't install the app, it will even open Google Play page to install the app.

How to craft such link? Take one of my favorite apps, Zhihu.

You could craft a link like this: `intent://www.zhihu.com/#Intent;scheme=https;category=android.intent.category.BROWSABLE;package=com.zhihu.android;end`

When users click it, it will open zhihu if users have installed zhihu, or open Google Play if they haven't.

It is easy to craft, you could run these code in Android studio:

```java
intent.action = Intent.ACTION_VIEW
intent.data = Uri.parse("https://www.zhihu.com/")
intent.addCategory(Intent.CATEGORY_BROWSABLE)
intent.setPackage("com.zhihu.android")
Log.d("The Url: ", intent.toUri(Intent.URI_INTENT_SCHEME))
```

Not only the odd link we just craft will jump to the app, even the link like `https://www.zhihu.com`, users still see app picker to open when they click the link.

Here are the questions:

* What the app developer need to do to let browser access your app when clicking a link
* What chromium does to support this feature, including normal link handling like `https://www.zhihu.com`

## App Developer Guide

As you can see from the craft code above:

* You need to provide an **Activity** component with
  * Action: VIEW
  * Category: BROWSABLE
  * exported: true (So that the component could be accessed by the browser app)

Just like this:

```
<activity-alias android:name="com.google.android.apps.chrome.IntentDispatcher"
    android:targetActivity="org.chromium.chrome.browser.document.ChromeLauncherActivity"
    android:exported="true">
    <intent-filter>
        <action android:name="android.intent.action.VIEW" />
        <category android:name="android.intent.category.DEFAULT" />
        <category android:name="android.intent.category.BROWSABLE" />
        <data android:scheme="http" />
    </intent-filter>
</activity-alias>
```

More specific `<intent-filter>` part will gain you more chance to beat the alternative app.

## What Chromium does

This is a feature call  [external_intents](https://source.chromium.org/chromium/chromium/src/+/main:components/external_intents/)

When a request is about to be called or redirected, chromium Android will transform the link into an Android Intent, query the system whether there is an app which could handle it. If no, the browser itself will just handle the link.

The query uses Android PackageManager.queryIntentActivities. 

And the whole flow would be:
![external_intents_workflow](res/external_intents_workflow.png)

You could check [ExternalNavigationHandler.java::shouldOverrideUrlLoading](https://source.chromium.org/chromium/chromium/src/+/main:components/external_intents/android/java/src/org/chromium/components/external_intents/ExternalNavigationHandler.java;drc=0c1cd8c71292d0093b983e30dc37a4ca81995aad;bpv=1;bpt=1;l=436?gsn=shouldOverrideUrlLoading&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Djava%3Fpath%3Dorg.chromium.components.external_intents.ExternalNavigationHandler%234818bee24fe148fccbc593fda9236e8a11107a923e87b255d086e8cf846097ad) for more information. Here we will talk more about how the link transforms into Intent.

Suppose you got an link like `intent://www.zhihu.com;blabla`, it will recognize the schema `intent`, and parse the uri into Intent:

```
Intent Intent::parseUri(uri)
```

If it is a normal link, it will be crafted into Intent:

```
intent = new Intent(Intent.ACTION_VIEW);
intent.setData(uri)
```

You may wonder, we often click normal link, why they never toke us to some other app. For example, you've installed edge/chrome, if any normal link will be crafted into an intent, and both edge/chrome could process that intent, why when we click one link in edge, it will not jump into chrome.

The tricky part is it will filter the WebApp. (That means the app developer should prevent their app as WebApp)

How to identify WebApp ?

Check the codes in [WebApkValidator.java::isValidWebApk](https://source.chromium.org/chromium/chromium/src/+/main:components/webapk/android/libs/client/src/org/chromium/components/webapk/lib/client/WebApkValidator.java;drc=b8524150039182faf7988e9478a9eff89728ac03;l=210?q=isValidWebApk&ss=chromium%2Fchromium%2Fsrc)

Frankly speaking, I just cannot imagine every link I click will go through such a long procedure, which is totally useless if I don't want to jump to any other apps.

## Security

You may wonder whether we could do something funny:

* could we craft links to access any exported Activity components?

Well, I was excited about it. That means no longer the guys have to persuade the victim to install a malicious app, just craft a link, victim clicks it, it is done.

However, the answer is no, only support few of them. Good job, Google.(Oh, bounties)

First, it is possible for people to craft any Intent to access any exported Activity components. You could follow my first code patch.

However, these intent will be sanitized before they're used. The sanitization will limit to access the components with  `CATEGORY_BROWSABLE`, which is quite rare in apps. So far it seems secure.

```java
// ExternalNavigationHandler.java    
public static void sanitizeQueryIntentActivitiesIntent(Intent intent) {
    intent.setFlags(intent.getFlags() & ALLOWED_INTENT_FLAGS);
    intent.addCategory(Intent.CATEGORY_BROWSABLE);
    intent.setComponent(null);
    // Intent Selectors allow intents to bypass the intent filter and potentially send apps URIs
    // they were not expecting to handle. https://crbug.com/1254422
    intent.setSelector(null);
}
```

## At last

DeepLink is an interesting feature. It connects the web and the app in Android. However, it will take a few cost on verification, and sadly it happens to all the links. So far, the web developer could only access the components with `CATEGORY_BROWSABLE`, which is quite limited.
