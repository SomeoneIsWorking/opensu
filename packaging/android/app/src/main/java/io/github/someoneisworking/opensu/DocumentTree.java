package io.github.someoneisworking.opensu;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import java.io.File;
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
        Document.COLUMN_SIZE,
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

    /** One row per child, "d|f TAB size TAB name TAB uri"; null when `location` is no readable folder. */
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
                Uri child = DocumentsContract.buildDocumentUriUsingTree(uri, cursor.getString(0));
                rows.add((folder ? "d" : "f") + "\t" + size + "\t" + cursor.getString(1) + "\t" + child);
            }
        } catch (SecurityException | IllegalArgumentException refused) {
            return null;
        }
        return rows.toArray(new String[0]);
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
