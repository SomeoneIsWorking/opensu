package io.github.someoneisworking.opensu;

import android.content.ContentResolver;
import android.content.UriPermission;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

/** Folders the player granted through the system picker, read by their document URIs. */
final class DocumentTree {
    private static final String[] COLUMNS = {
        Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE,
        Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED,
    };

    private final Context context;

    DocumentTree(Context context) {
        this.context = context;
    }

    static boolean isDocument(String location) {
        return location.startsWith(ContentResolver.SCHEME_CONTENT + "://");
    }

    /** The document `uri` names: a picked tree's root or a document inside it. */
    private String documentId(Uri uri) {
        return DocumentsContract.isDocumentUri(context, uri)
                ? DocumentsContract.getDocumentId(uri)
                : DocumentsContract.getTreeDocumentId(uri);
    }

    /** One row per child, "d|f TAB size TAB modified TAB name TAB uri"; null when `location` is no readable folder. */
    String[] children(String location) {
        Uri uri = Uri.parse(location);
        Uri listing;
        try {
            listing = DocumentsContract.buildChildDocumentsUriUsingTree(uri, documentId(uri));
        } catch (IllegalArgumentException notTree) {
            return null;
        }
        List<String> rows = new ArrayList<>();
        try (Cursor cursor = context.getContentResolver().query(listing, COLUMNS, null, null, null)) {
            if (cursor == null) {
                return null;
            }
            while (cursor.moveToNext()) {
                boolean folder = Document.MIME_TYPE_DIR.equals(cursor.getString(2));
                long size = cursor.isNull(3) ? 0 : cursor.getLong(3);
                long modified = cursor.isNull(4) ? 0 : cursor.getLong(4);
                Uri child = DocumentsContract.buildDocumentUriUsingTree(uri, cursor.getString(0));
                rows.add((folder ? "d" : "f") + "\t" + size + "\t" + modified + "\t" + cursor.getString(1)
                        + "\t" + child);
            }
        } catch (SecurityException | IllegalArgumentException refused) {
            return null;
        }
        return rows.toArray(new String[0]);
    }

    /** The document open for reading: a descriptor the caller closes, or -1 when it cannot be opened. */
    int open(String location) {
        try {
            ParcelFileDescriptor file =
                    context.getContentResolver().openFileDescriptor(Uri.parse(location), "r");
            return file == null ? -1 : file.detachFd();
        } catch (FileNotFoundException | SecurityException | IllegalArgumentException refused) {
            return -1;
        }
    }

    /** A picked folder or a document in it as the player reads it: "ROMs" or "ROMs/nes". */
    String label(String location) {
        Uri uri = Uri.parse(location);
        try {
            Uri document = DocumentsContract.buildDocumentUriUsingTree(uri, documentId(uri));
            return name(document);
        } catch (IllegalArgumentException | SecurityException unreadable) {
            return location;
        }
    }

    /** The document's display name, or its last path segment when the provider gives none. */
    String name(Uri uri) {
        try (Cursor cursor = context.getContentResolver().query(
                uri, new String[] {Document.COLUMN_DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                return cursor.getString(0);
            }
        }
        return uri.getLastPathSegment();
    }

    /** Whether the app kept a grant to write in the picked tree `location`. */
    boolean writable(String location) {
        Uri tree = Uri.parse(location);
        for (UriPermission grant : context.getContentResolver().getPersistedUriPermissions()) {
            if (grant.getUri().equals(tree) && grant.isWritePermission()) {
                return true;
            }
        }
        return false;
    }

    /** Gives back the grant kept on the picked tree `location`, when one is held. */
    void release(String location) {
        Uri tree = Uri.parse(location);
        ContentResolver resolver = context.getContentResolver();
        for (UriPermission grant : resolver.getPersistedUriPermissions()) {
            if (grant.getUri().equals(tree)) {
                int modes = (grant.isReadPermission() ? Intent.FLAG_GRANT_READ_URI_PERMISSION : 0)
                        | (grant.isWritePermission() ? Intent.FLAG_GRANT_WRITE_URI_PERMISSION : 0);
                resolver.releasePersistableUriPermission(tree, modes);
                return;
            }
        }
    }

    /** The child of `parent` (a document of `tree`) named `name`, or null. */
    private Uri child(Uri tree, Uri parent, String name) {
        Uri listing = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId(parent));
        try (Cursor cursor = context.getContentResolver().query(listing, COLUMNS, null, null, null)) {
            while (cursor != null && cursor.moveToNext()) {
                if (name.equals(cursor.getString(1))) {
                    return DocumentsContract.buildDocumentUriUsingTree(tree, cursor.getString(0));
                }
            }
        }
        return null;
    }

    /**
     * Copies `file` to `relative` ("nes/Game.nes") under the picked tree `location`, making the
     * folders on the way and replacing a document of that name: null once it is there, else why not.
     */
    String place(String location, String relative, String file) {
        Uri tree = Uri.parse(location);
        ContentResolver resolver = context.getContentResolver();
        String[] steps = relative.split("/");
        try {
            Uri parent = DocumentsContract.buildDocumentUriUsingTree(tree, documentId(tree));
            for (int i = 0; i < steps.length - 1; ++i) {
                Uri folder = child(tree, parent, steps[i]);
                if (folder == null) {
                    folder = DocumentsContract.createDocument(resolver, parent, Document.MIME_TYPE_DIR, steps[i]);
                }
                if (folder == null) {
                    return "cannot make the folder " + steps[i];
                }
                parent = folder;
            }
            String name = steps[steps.length - 1];
            Uri old = child(tree, parent, name);
            if (old != null) {
                DocumentsContract.deleteDocument(resolver, old);
            }
            Uri target = DocumentsContract.createDocument(resolver, parent, "application/octet-stream", name);
            if (target == null) {
                return "cannot make " + relative;
            }
            try (InputStream in = new java.io.FileInputStream(file);
                 OutputStream out = resolver.openOutputStream(target, "w")) {
                if (out == null) {
                    return "cannot write " + relative;
                }
                byte[] buffer = new byte[1 << 16];
                for (int read = in.read(buffer); read >= 0; read = in.read(buffer)) {
                    out.write(buffer, 0, read);
                }
            }
            return null;
        } catch (IOException | SecurityException | IllegalArgumentException | IllegalStateException refused) {
            return refused.toString();
        }
    }

    /** Copies the document into `folder` under its own name, for programs that take a file path. */
    File stage(String location, File folder) throws IOException {
        Uri uri = Uri.parse(location);
        File target = new File(folder, new File(name(uri)).getName());
        try (InputStream in = context.getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(target)) {
            if (in == null) {
                throw new IOException("cannot open " + location);
            }
            byte[] buffer = new byte[1 << 16];
            for (int read = in.read(buffer); read >= 0; read = in.read(buffer)) {
                out.write(buffer, 0, read);
            }
        }
        return target;
    }
}
